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

#include <WebCore/DDImageAsset.h>
#include <WebCore/DDMaterialDescriptor.h>
#include <WebCore/DDMesh.h>
#include <WebCore/DDMeshDescriptor.h>
#include <WebCore/DDTextureDescriptor.h>
#include <WebCore/DDUpdateMaterialDescriptor.h>
#include <WebCore/DDUpdateMeshDescriptor.h>
#include <WebCore/DDUpdateTextureDescriptor.h>
#include <WebCore/ModelDDTypes.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/cocoa/VectorCocoa.h>

namespace WebCore {

static String toCpp(NSString* s)
{
    return String(s);
}

static DDModel::DDVertexAttributeFormat toCpp(WebDDVertexAttributeFormat *format)
{
    return DDModel::DDVertexAttributeFormat {
        .semantic = format.semantic,
        .format = format.format,
        .layoutIndex = format.layoutIndex,
        .offset = format.offset
    };
}

static Vector<DDModel::DDVertexAttributeFormat> toCppVertexAttributes(NSArray<WebDDVertexAttributeFormat *> *formats)
{
    Vector<DDModel::DDVertexAttributeFormat> result;
    for (WebDDVertexAttributeFormat *f in formats)
        result.append(toCpp(f));
    return result;
}

static DDModel::DDVertexLayout toCpp(WebDDVertexLayout *layout)
{
    return DDModel::DDVertexLayout {
        .bufferIndex = layout.bufferIndex,
        .bufferOffset = layout.bufferOffset,
        .bufferStride = layout.bufferStride,
    };
}
static Vector<DDModel::DDVertexLayout> toCppVertexLayouts(NSArray<WebDDVertexLayout *> *layouts)
{
    Vector<DDModel::DDVertexLayout> result;
    for (WebDDVertexLayout *l in layouts)
        result.append(toCpp(l));
    return result;
}

static WebCore::DDModel::DDMeshDescriptor toCpp(WebAddMeshRequest *addMesh)
{
    return WebCore::DDModel::DDMeshDescriptor {
        .indexCapacity = addMesh.indexCapacity,
        .indexType = addMesh.indexType,
        .vertexBufferCount = addMesh.vertexBufferCount,
        .vertexCapacity = addMesh.vertexCapacity,
        .vertexAttributes = toCppVertexAttributes(addMesh.vertexAttributes),
        .vertexLayouts = toCppVertexLayouts(addMesh.vertexLayouts),
        .identifier = addMesh.identifier
    };
}

static Vector<DDModel::DDFloat4x4> toVector(WebChainedFloat4x4 *input)
{
    Vector<DDModel::DDFloat4x4> result;
    for ( ; input; input = input.next)
        result.append(input.transform);

    return result;
}

static Vector<KeyValuePair<int32_t, uint64_t>> toCpp(NSArray<WebSetRenderFlags *> *renderFlags)
{
    Vector<KeyValuePair<int32_t, uint64_t>> result;
    for (WebSetRenderFlags *flag in renderFlags)
        result.append({ flag.partIndex, flag.renderFlags });
    return result;
}

static DDModel::DDMeshPart toCpp(WebDDMeshPart *part)
{
    return DDModel::DDMeshPart {
        static_cast<uint32_t>(part.indexOffset),
        static_cast<uint32_t>(part.indexCount),
        static_cast<uint32_t>(part.topology),
        static_cast<uint32_t>(part.materialIndex),
        WTFMove(part.boundsMin),
        WTFMove(part.boundsMax)
    };
}

static Vector<KeyValuePair<int32_t, DDModel::DDMeshPart>> toCpp(NSArray<WebSetPart *> *parts)
{
    Vector<KeyValuePair<int32_t, DDModel::DDMeshPart>> result;
    for (WebSetPart *f in parts)
        result.append({ f.partIndex, toCpp(f.part) });
    return result;
}

static DDModel::DDReplaceVertices toCpp(WebReplaceVertices *a)
{
    return DDModel::DDReplaceVertices {
        .bufferIndex = static_cast<int32_t>(a.bufferIndex),
        .buffer = makeVector(a.buffer)
    };
}

static Vector<DDModel::DDReplaceVertices> toCpp(NSArray<WebReplaceVertices *> *arr)
{
    Vector<DDModel::DDReplaceVertices> result;
    for (WebReplaceVertices *a in arr)
        result.append(toCpp(a));
    return result;
}

static Vector<String> toCpp(NSArray<NSUUID *> *arr)
{
    Vector<String> result;
    for (NSUUID *a in arr) {
        if (a)
            result.append(String(a.UUIDString));
    }
    return result;
}

static WebCore::DDModel::DDUpdateMeshDescriptor toCpp(WebUpdateMeshRequest *update)
{
    return WebCore::DDModel::DDUpdateMeshDescriptor {
        .partCount = static_cast<int32_t>(update.partCount),
        .parts = toCpp(update.parts),
        .renderFlags = toCpp(update.renderFlags),
        .vertices = toCpp(update.vertices),
        .indices = makeVector(update.indices),
        .transform = update.transform,
        .instanceTransforms4x4 = toVector(update.instanceTransforms),
        .materialIds = toCpp(update.materialIds),
        .identifier = update.identifier
    };
}

static WebCore::DDModel::DDSemantic toCpp(WebDDSemantic semantic)
{
    switch (semantic) {
    case WebDDSemantic::kColor:
        return WebCore::DDModel::DDSemantic::Color;
    case WebDDSemantic::kVector:
        return WebCore::DDModel::DDSemantic::Vector;
    case WebDDSemantic::kScalar:
        return WebCore::DDModel::DDSemantic::Scalar;
    default:
    case WebDDSemantic::kUnknown:
        return WebCore::DDModel::DDSemantic::Unknown;
    }
}

static WebCore::DDModel::DDImageAsset toCpp(WebDDImageAsset *imageAsset)
{
    RetainPtr imageSource = adoptCF(CGImageSourceCreateWithData((CFDataRef)imageAsset.data, nullptr));
    auto platformImage = adoptCF(CGImageSourceCreateImageAtIndex(imageSource.get(), 0, nullptr));
    RetainPtr pixelDataCfData = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(platformImage.get())));
    auto byteSpan = span(pixelDataCfData.get());

    auto width = CGImageGetWidth(platformImage.get());
    auto height = CGImageGetHeight(platformImage.get());
    auto bytesPerPixel = (int)(byteSpan.size() / (width * height));

    return WebCore::DDModel::DDImageAsset {
        .data = Vector<uint8_t> { byteSpan },
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .bytesPerPixel = static_cast<uint32_t>(bytesPerPixel),
        .semantic = toCpp(imageAsset.semantic),
        .path = imageAsset.path,
        .identifier = imageAsset.identifier
    };
}

static WebCore::DDModel::DDNodeType toCpp(WebDDNodeType nodeType)
{
    switch (nodeType) {
    case WebDDNodeType::kBuiltin:
        return WebCore::DDModel::DDNodeType::Builtin;
    case WebDDNodeType::kConstant:
        return WebCore::DDModel::DDNodeType::Constant;
    case WebDDNodeType::kArguments:
        return WebCore::DDModel::DDNodeType::Arguments;
    default:
    case WebDDNodeType::kResults:
        return WebCore::DDModel::DDNodeType::Results;
    }
}

static WebCore::DDModel::DDBuiltin toCpp(WebDDBuiltin *builtin)
{
    return WebCore::DDModel::DDBuiltin {
        .definition = toCpp(builtin.definition),
        .name = toCpp(builtin.name)
    };
}

static WebCore::DDModel::DDConstant toCpp(WebDDConstant constant)
{
    switch (constant) {
    case WebDDConstant::kBool:
        return WebCore::DDModel::DDConstant::kBool;
    case WebDDConstant::kUchar:
        return WebCore::DDModel::DDConstant::kUchar;
    case WebDDConstant::kInt:
        return WebCore::DDModel::DDConstant::kInt;
    case WebDDConstant::kUint:
        return WebCore::DDModel::DDConstant::kUint;
    case WebDDConstant::kHalf:
        return WebCore::DDModel::DDConstant::kHalf;
    case WebDDConstant::kFloat:
        return WebCore::DDModel::DDConstant::kFloat;
    case WebDDConstant::kTimecode:
        return WebCore::DDModel::DDConstant::kTimecode;
    case WebDDConstant::kString:
        return WebCore::DDModel::DDConstant::kString;
    case WebDDConstant::kToken:
        return WebCore::DDModel::DDConstant::kToken;
    case WebDDConstant::kAsset:
        return WebCore::DDModel::DDConstant::kAsset;
    case WebDDConstant::kMatrix2f:
        return WebCore::DDModel::DDConstant::kMatrix2f;
    case WebDDConstant::kMatrix3f:
        return WebCore::DDModel::DDConstant::kMatrix3f;
    case WebDDConstant::kMatrix4f:
        return WebCore::DDModel::DDConstant::kMatrix4f;
    case WebDDConstant::kQuatf:
        return WebCore::DDModel::DDConstant::kQuatf;
    case WebDDConstant::kQuath:
        return WebCore::DDModel::DDConstant::kQuath;
    case WebDDConstant::kFloat2:
        return WebCore::DDModel::DDConstant::kFloat2;
    case WebDDConstant::kHalf2:
        return WebCore::DDModel::DDConstant::kHalf2;
    case WebDDConstant::kInt2:
        return WebCore::DDModel::DDConstant::kInt2;
    case WebDDConstant::kFloat3:
        return WebCore::DDModel::DDConstant::kFloat3;
    case WebDDConstant::kHalf3:
        return WebCore::DDModel::DDConstant::kHalf3;
    case WebDDConstant::kInt3:
        return WebCore::DDModel::DDConstant::kInt3;
    case WebDDConstant::kFloat4:
        return WebCore::DDModel::DDConstant::kFloat4;
    case WebDDConstant::kHalf4:
        return WebCore::DDModel::DDConstant::kHalf4;
    case WebDDConstant::kInt4:
        return WebCore::DDModel::DDConstant::kInt4;

    case WebDDConstant::kPoint3f:
        return WebCore::DDModel::DDConstant::kPoint3f;
    case WebDDConstant::kPoint3h:
        return WebCore::DDModel::DDConstant::kPoint3h;
    case WebDDConstant::kNormal3f:
        return WebCore::DDModel::DDConstant::kNormal3f;
    case WebDDConstant::kNormal3h:
        return WebCore::DDModel::DDConstant::kNormal3h;
    case WebDDConstant::kVector3f:
        return WebCore::DDModel::DDConstant::kVector3f;
    case WebDDConstant::kVector3h:
        return WebCore::DDModel::DDConstant::kVector3h;
    case WebDDConstant::kColor3f:
        return WebCore::DDModel::DDConstant::kColor3f;
    case WebDDConstant::kColor3h:
        return WebCore::DDModel::DDConstant::kColor3h;
    case WebDDConstant::kColor4f:
        return WebCore::DDModel::DDConstant::kColor4f;
    case WebDDConstant::kColor4h:
        return WebCore::DDModel::DDConstant::kColor4h;
    case WebDDConstant::kTexCoord2h:
        return WebCore::DDModel::DDConstant::kTexCoord2h;
    case WebDDConstant::kTexCoord2f:
        return WebCore::DDModel::DDConstant::kTexCoord2f;
    case WebDDConstant::kTexCoord3h:
        return WebCore::DDModel::DDConstant::kTexCoord3h;
    case WebDDConstant::kTexCoord3f:
        return WebCore::DDModel::DDConstant::kTexCoord3f;
    }
}

static Vector<WebCore::DDModel::DDNumberOrString> toCpp(NSArray<NSValue *> *constantValues)
{
    Vector<WebCore::DDModel::DDNumberOrString> result;
    result.reserveCapacity(constantValues.count);
    for (NSValue* v in constantValues) {
        if ([v isKindOfClass:NSNumber.class])
            result.append(static_cast<NSNumber *>(v).doubleValue);
        else
            result.append(String(static_cast<NSString*>(v.nonretainedObjectValue)));
    }

    return result;
}

static WebCore::DDModel::DDConstantContainer toCpp(WebDDConstantContainer *container)
{
    return WebCore::DDModel::DDConstantContainer {
        .constant = toCpp(container.constant),
        .constantValues = toCpp(container.constantValues),
        .name = toCpp(container.name)
    };
}

static WebCore::DDModel::DDNode toCpp(WebDDNode *node)
{
    return WebCore::DDModel::DDNode {
        .bridgeNodeType = toCpp(node.bridgeNodeType),
        .builtin = toCpp(node.builtin),
        .constant = toCpp(node.constant)
    };
}

static WebCore::DDModel::DDEdge toCpp(WebDDEdge *edge)
{
    return WebCore::DDModel::DDEdge {
        .upstreamNodeIndex = edge.upstreamNodeIndex,
        .downstreamNodeIndex = edge.downstreamNodeIndex,
        .upstreamOutputName = toCpp(edge.upstreamOutputName),
        .downstreamInputName = toCpp(edge.downstreamInputName)
    };
}

static WebCore::DDModel::DDDataType toCpp(WebDDDataType type)
{
    switch (type) {
    case WebDDDataType::kBool:
        return WebCore::DDModel::DDDataType::kBool;
    case WebDDDataType::kInt:
        return WebCore::DDModel::DDDataType::kInt;
    case WebDDDataType::kInt2:
        return WebCore::DDModel::DDDataType::kInt2;
    case WebDDDataType::kInt3:
        return WebCore::DDModel::DDDataType::kInt3;
    case WebDDDataType::kInt4:
        return WebCore::DDModel::DDDataType::kInt4;
    case WebDDDataType::kFloat:
        return WebCore::DDModel::DDDataType::kFloat;
    case WebDDDataType::kColor3f:
        return WebCore::DDModel::DDDataType::kColor3f;
    case WebDDDataType::kColor3h:
        return WebCore::DDModel::DDDataType::kColor3h;
    case WebDDDataType::kColor4f:
        return WebCore::DDModel::DDDataType::kColor4f;
    case WebDDDataType::kColor4h:
        return WebCore::DDModel::DDDataType::kColor4h;
    case WebDDDataType::kFloat2:
        return WebCore::DDModel::DDDataType::kFloat2;
    case WebDDDataType::kFloat3:
        return WebCore::DDModel::DDDataType::kFloat3;
    case WebDDDataType::kFloat4:
        return WebCore::DDModel::DDDataType::kFloat4;
    case WebDDDataType::kHalf:
        return WebCore::DDModel::DDDataType::kHalf;
    case WebDDDataType::kHalf2:
        return WebCore::DDModel::DDDataType::kHalf2;
    case WebDDDataType::kHalf3:
        return WebCore::DDModel::DDDataType::kHalf3;
    case WebDDDataType::kHalf4:
        return WebCore::DDModel::DDDataType::kHalf4;
    case WebDDDataType::kMatrix2f:
        return WebCore::DDModel::DDDataType::kMatrix2f;
    case WebDDDataType::kMatrix3f:
        return WebCore::DDModel::DDDataType::kMatrix3f;
    case WebDDDataType::kMatrix4f:
        return WebCore::DDModel::DDDataType::kMatrix4f;
    case WebDDDataType::kSurfaceShader:
        return WebCore::DDModel::DDDataType::kSurfaceShader;
    case WebDDDataType::kGeometryModifier:
        return WebCore::DDModel::DDDataType::kGeometryModifier;
    case WebDDDataType::kString:
        return WebCore::DDModel::DDDataType::kString;
    case WebDDDataType::kToken:
        return WebCore::DDModel::DDDataType::kToken;
    case WebDDDataType::kAsset:
        return WebCore::DDModel::DDDataType::kAsset;
    default:
        RELEASE_ASSERT_NOT_REACHED("USD file is corrupt");
    }
}

static WebCore::DDModel::DDInputOutput toCpp(WebDDInputOutput *inputOutput)
{
    return WebCore::DDModel::DDInputOutput {
        .type = toCpp(inputOutput.type),
        .name = toCpp(inputOutput.name)
    };
}

static WebCore::DDModel::DDPrimvar toCpp(WebDDPrimvar *primvar)
{
    return WebCore::DDModel::DDPrimvar {
        .name = toCpp(primvar.name),
        .referencedGeomPropName = toCpp(primvar.referencedGeomPropName),
        .attributeFormat = primvar.attributeFormat
    };
}

template<typename T, typename U>
static Vector<U> toCpp(NSArray<T *> *nsArray)
{
    Vector<U> result;
    result.reserveCapacity(nsArray.count);
    for (T *v in nsArray)
        result.append(toCpp(v));

    return result;
}

static WebCore::DDModel::DDMaterialGraph toCpp(WebDDMaterialGraph *materialGraph)
{
    return WebCore::DDModel::DDMaterialGraph {
        .nodes = toCpp<WebDDNode, WebCore::DDModel::DDNode>(materialGraph.nodes),
        .edges = toCpp<WebDDEdge, WebCore::DDModel::DDEdge>(materialGraph.edges),
        .inputs = toCpp<WebDDInputOutput, WebCore::DDModel::DDInputOutput>(materialGraph.inputs),
        .outputs = toCpp<WebDDInputOutput, WebCore::DDModel::DDInputOutput>(materialGraph.outputs),
        .primvars = toCpp<WebDDPrimvar, WebCore::DDModel::DDPrimvar>(materialGraph.primvars),
        .identifier = materialGraph.identifier
    };
}

static WebCore::DDModel::DDTextureDescriptor toCpp(WebDDAddTextureRequest *update)
{
    return WebCore::DDModel::DDTextureDescriptor {
        .imageAsset = toCpp(update.imageAsset)
    };
}

static WebCore::DDModel::DDUpdateTextureDescriptor toCpp(WebDDUpdateTextureRequest *update)
{
    return WebCore::DDModel::DDUpdateTextureDescriptor {
        .imageAsset = toCpp(update.imageAsset)
    };
}

static WebCore::DDModel::DDMaterialDescriptor toCpp(WebDDAddMaterialRequest *update)
{
    return WebCore::DDModel::DDMaterialDescriptor {
        .materialGraph = toCpp(update.material)
    };
}

static WebCore::DDModel::DDUpdateMaterialDescriptor toCpp(WebDDUpdateMaterialRequest *update)
{
    return WebCore::DDModel::DDUpdateMaterialDescriptor {
        .materialGraph = toCpp(update.material)
    };
}

}
