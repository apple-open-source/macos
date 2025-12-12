// Copyright (C) 2024 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import Foundation
import Metal
import QuartzCore
import WebGPU_Internal

#if canImport(DirectDrawBackend)
import DirectDrawBackend
import DirectDrawXPCTypes
import DirectResource
import ShaderGraph
import USDDirectDrawTypes
#endif

@objc
@implementation
extension DDBridgeVertexAttributeFormat {
    let semantic: Int32
    let format: Int32
    let layoutIndex: Int32
    let offset: Int32

    init(
        semantic: Int32,
        format: Int32,
        layoutIndex: Int32,
        offset: Int32
    ) {
        self.semantic = semantic
        self.format = format
        self.layoutIndex = layoutIndex
        self.offset = offset
    }
}

@objc
@implementation
extension DDBridgeVertexLayout {
    let bufferIndex: Int32
    let bufferOffset: Int32
    let bufferStride: Int32

    init(
        bufferIndex: Int32,
        bufferOffset: Int32,
        bufferStride: Int32
    ) {
        self.bufferIndex = bufferIndex
        self.bufferOffset = bufferOffset
        self.bufferStride = bufferStride
    }
}

@objc
@implementation
extension DDBridgeAddMeshRequest {
    let indexCapacity: Int32
    let indexType: Int32
    let vertexBufferCount: Int32
    let vertexCapacity: Int32
    let vertexAttributes: [DDBridgeVertexAttributeFormat]?
    let vertexLayouts: [DDBridgeVertexLayout]?

    init(
        indexCapacity: Int32,
        indexType: Int32,
        vertexBufferCount: Int32,
        vertexCapacity: Int32,
        vertexAttributes: [DDBridgeVertexAttributeFormat]?,
        vertexLayouts: [DDBridgeVertexLayout]?
    ) {
        self.indexCapacity = indexCapacity
        self.indexType = indexType
        self.vertexBufferCount = vertexBufferCount
        self.vertexCapacity = vertexCapacity
        self.vertexAttributes = vertexAttributes
        self.vertexLayouts = vertexLayouts
    }

    #if canImport(DirectDrawBackend)
    fileprivate var reDescriptor: DirectResource.MeshDescriptor {
        let descriptor = DirectResource.MeshDescriptor()

        descriptor.indexCapacity = DRInteger(indexCapacity)
        descriptor.indexType = MTLIndexType(rawValue: UInt(indexType)) ?? .uint16
        descriptor.vertexBufferCount = DRInteger(vertexBufferCount)
        descriptor.vertexCapacity = DRInteger(vertexCapacity)
        if let vertexAttributes {
            descriptor.vertexAttributeCount = vertexAttributes.count
            for (index, attribute) in vertexAttributes.enumerated() {
                descriptor.setVertexAttribute(
                    index: UInt(index),
                    semantic: DRVertexSemantic(rawValue: attribute.semantic) ?? .invalid,
                    format: MTLVertexFormat(rawValue: UInt(attribute.format)) ?? .invalid,
                    layoutIndex: DRUInteger(attribute.layoutIndex),
                    offset: DRUInteger(attribute.offset)
                )
            }
        }
        if let vertexLayouts {
            descriptor.vertexLayoutCount = vertexLayouts.count
            for (index, layout) in vertexLayouts.enumerated() {
                descriptor.setVertexLayout(
                    layoutIndex: UInt(index),
                    bufferIndex: DRUInteger(layout.bufferIndex), // RE ensures this is within bounds
                    bufferOffset: DRUInteger(layout.bufferOffset),
                    bufferStride: DRUInteger(layout.bufferStride)
                )
            }
        }
        return descriptor
    }
    #endif
}

@objc
@implementation
extension DDBridgeMeshPart {
    let indexOffset: UInt
    let indexCount: UInt
    let topology: UInt
    let materialIndex: UInt
    let boundsMin: simd_float3
    let boundsMax: simd_float3

    init(
        indexOffset: UInt,
        indexCount: UInt,
        topology: UInt,
        materialIndex: UInt,
        boundsMin: simd_float3,
        boundsMax: simd_float3
    ) {
        self.indexOffset = indexOffset
        self.indexCount = indexCount
        self.topology = topology
        self.materialIndex = materialIndex
        self.boundsMin = boundsMin
        self.boundsMax = boundsMax
    }
}

@objc
@implementation
extension DDBridgeSetPart {
    let partIndex: Int
    let part: DDBridgeMeshPart

    init(
        index: Int,
        part: DDBridgeMeshPart
    ) {
        self.partIndex = index
        self.part = part
    }
}

@objc
@implementation
extension DDBridgeSetRenderFlags {
    let partIndex: Int
    let renderFlags: UInt64

    init(
        index: Int,
        renderFlags: UInt64
    ) {
        self.partIndex = index
        self.renderFlags = renderFlags
    }
}

@objc
@implementation
extension DDBridgeReplaceVertices {
    let bufferIndex: Int
    let buffer: Data

    init(
        bufferIndex: Int,
        buffer: Data
    ) {
        self.bufferIndex = bufferIndex
        self.buffer = buffer
    }
}

@objc
@implementation
extension DDBridgeChainedFloat4x4 {
    var transform: simd_float4x4
    var next: DDBridgeChainedFloat4x4?

    init(
        transform: simd_float4x4
    ) {
        self.transform = transform
    }
}

@objc
@implementation
extension DDBridgeUpdateMesh {
    let partCount: Int
    let parts: [DDBridgeSetPart]
    let renderFlags: [DDBridgeSetRenderFlags]
    let vertices: [DDBridgeReplaceVertices]
    let indices: Data?
    let transform: simd_float4x4
    var instanceTransforms: DDBridgeChainedFloat4x4? // array of float4x4
    let materialIds: [UUID]

    init(
        partCount: Int,
        parts: [DDBridgeSetPart],
        renderFlags: [DDBridgeSetRenderFlags],
        vertices: [DDBridgeReplaceVertices],
        indices: Data?,
        transform: simd_float4x4,
        instanceTransforms: DDBridgeChainedFloat4x4?,
        materialIds: [UUID]
    ) {
        self.partCount = partCount
        self.parts = parts
        self.renderFlags = renderFlags
        self.vertices = vertices
        self.indices = indices
        self.transform = transform
        self.instanceTransforms = instanceTransforms
        self.materialIds = materialIds
    }
}

@objc
@implementation
extension DDBridgeImageAsset {
    let data: Data?
    let width: UInt
    let height: UInt
    let bytesPerPixel: UInt
    let semantic: DDBridgeSemantic
    let path: String

    init(
        data: Data?,
        width: UInt,
        height: UInt,
        bytesPerPixel: UInt,
        semantic: DDBridgeSemantic,
        path: String
    ) {
        self.data = data
        self.width = width
        self.height = height
        self.bytesPerPixel = bytesPerPixel
        self.semantic = semantic
        self.path = path
    }
}

@objc
@implementation
extension DDBridgeAddTextureRequest {
    let imageAsset: DDBridgeImageAsset?

    @objc(initWithImageAsset:)
    init(
        imageAsset: DDBridgeImageAsset?
    ) {
        self.imageAsset = imageAsset
    }
}

@objc
@implementation
extension DDBridgeUpdateTextureRequest {
    let imageAsset: DDBridgeImageAsset?

    @objc(initWithImageAsset:)
    init(
        imageAsset: DDBridgeImageAsset?
    ) {
        self.imageAsset = imageAsset
    }
}

@objc
@implementation
extension DDBridgeUpdateMaterialRequest {
    let material: DDBridgeMaterialGraph

    @objc(initWithMaterial:)
    init(
        material: DDBridgeMaterialGraph
    ) {
        self.material = material
    }
}

@objc
@implementation
extension DDBridgeAddMaterialRequest {
    let material: DDBridgeMaterialGraph

    @objc(initWithMaterial:)
    init(
        material: DDBridgeMaterialGraph
    ) {
        self.material = material
    }
}

@objc
@implementation
extension DDBridgeNode {
    let bridgeNodeType: DDBridgeNodeType
    let builtin: DDBridgeBuiltin
    let constant: DDBridgeConstantContainer

    init(
        bridgeNodeType: DDBridgeNodeType,
        builtin: DDBridgeBuiltin,
        constant: DDBridgeConstantContainer
    ) {
        self.bridgeNodeType = bridgeNodeType
        self.builtin = builtin
        self.constant = constant
    }
}

@objc
@implementation
extension DDBridgeInputOutput {
    let type: DDBridgeDataType
    let name: String

    init(
        type: DDBridgeDataType,
        name: String
    ) {
        self.type = type
        self.name = name
    }
}

@objc
@implementation
extension DDBridgePrimvar {
    let name: String
    let referencedGeomPropName: String
    let attributeFormat: UInt

    init(
        name: String,
        referencedGeomPropName: String,
        attributeFormat: UInt
    ) {
        self.name = name
        self.referencedGeomPropName = referencedGeomPropName
        self.attributeFormat = attributeFormat
    }
}

@objc
@implementation
extension DDBridgeConstantContainer {
    let constant: DDBridgeConstant
    let constantValues: [DDValueString]
    let name: String

    init(
        constant: DDBridgeConstant,
        constantValues: [DDValueString],
        name: String
    ) {
        self.constant = constant
        self.constantValues = constantValues
        self.name = name
    }
}

@objc
@implementation
extension DDBridgeBuiltin {
    let definition: String
    let name: String

    init(
        definition: String,
        name: String
    ) {
        self.definition = definition
        self.name = name
    }
}

@objc
@implementation
extension DDBridgeEdge {
    let upstreamNodeIndex: Int
    let downstreamNodeIndex: Int
    let upstreamOutputName: String
    let downstreamInputName: String

    init(
        upstreamNodeIndex: Int,
        downstreamNodeIndex: Int,
        upstreamOutputName: String,
        downstreamInputName: String
    ) {
        self.upstreamNodeIndex = upstreamNodeIndex
        self.downstreamNodeIndex = downstreamNodeIndex
        self.upstreamOutputName = upstreamOutputName
        self.downstreamInputName = downstreamInputName
    }
}

@objc
@implementation
extension DDBridgeMaterialGraph {
    let nodes: [DDBridgeNode]
    let edges: [DDBridgeEdge]
    let inputs: [DDBridgeInputOutput]
    let outputs: [DDBridgeInputOutput]
    let primvars: [DDBridgePrimvar]

    init(
        nodes: [DDBridgeNode],
        edges: [DDBridgeEdge],
        inputs: [DDBridgeInputOutput],
        outputs: [DDBridgeInputOutput],
        primvars: [DDBridgePrimvar],
    ) {
        self.nodes = nodes
        self.edges = edges
        self.inputs = inputs
        self.outputs = outputs
        self.primvars = primvars
    }
}

@objc
@implementation
extension DDValueString {
    let number: NSNumber
    let string: String

    init(
        string: String
    ) {
        self.number = NSNumber(value: 0)
        self.string = string
    }

    init(
        number: NSNumber
    ) {
        self.number = number
        self.string = ""
    }
}

extension MTLCaptureDescriptor {
    fileprivate convenience init(from device: MTLDevice?) {
        self.init()

        captureObject = device
        destination = .gpuTraceDocument
        let now = Date()
        let dateFormatter = DateFormatter()
        dateFormatter.timeZone = .current
        dateFormatter.dateFormat = "yyyy-MM-dd-HH-mm-ss-SSSS"
        let dateString = dateFormatter.string(from: now)

        outputURL = URL.temporaryDirectory.appending(path: "capture_\(dateString).gputrace").standardizedFileURL
    }
}

#if canImport(DirectDrawBackend)
class NodeStore {
    var nodeDefinitionStore: ShaderGraph.NodeDefinitionStore
    init() {
        nodeDefinitionStore = NodeDefinitionStore()
    }
}
#endif

class Helper {
    static fileprivate func isNonZero(value: Float) -> Bool {
        abs(value) > Float.ulpOfOne
    }

    static fileprivate func isNonZero(_ vector: simd_float4) -> Bool {
        isNonZero(value: vector[0]) || isNonZero(value: vector[1]) || isNonZero(value: vector[2]) || isNonZero(value: vector[3])
    }

    static fileprivate func isNonZero(matrix: simd_float4x4) -> Bool {
        isNonZero(_: matrix.columns.0) || isNonZero(_: matrix.columns.1) || isNonZero(_: matrix.columns.2) || isNonZero(_: matrix.columns.3)
    }

    #if canImport(DirectDrawBackend)
    static fileprivate func makeDirectTextureDescriptorFromMTLTexture(_ texture: MTLTexture) -> DirectResource.TextureDescriptor {
        let desciptor = DirectResource.TextureDescriptor()
        desciptor.height = Int(texture.height)
        desciptor.width = Int(texture.width)
        desciptor.pixelFormat = texture.pixelFormat
        desciptor.textureType = texture.textureType
        desciptor.textureUsage = texture.usage
        desciptor.swizzle = texture.swizzle
        desciptor.mipmapLevelCount = Int(log2(Double(max(texture.width, texture.height))) + 1.0)

        return desciptor
    }

    static fileprivate func copyTextureToDirectTexture(
        _ directTexture: DirectResource.Texture,
        sourceTexture: MTLTexture,
        commandQueue: MTLCommandQueue?
    ) {
        guard let commandQueue = commandQueue,
            let commandBuffer = commandQueue.makeCommandBuffer(),
            let blitEncoder = commandBuffer.makeBlitCommandEncoder()
        else {
            return
        }

        let outTexture = directTexture.replace(commandBuffer: commandBuffer)
        blitEncoder.copy(from: sourceTexture, to: outTexture)
        blitEncoder.generateMipmaps(for: outTexture)
        blitEncoder.endEncoding()
        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()
    }

    static fileprivate func makeTextureFromImageAsset(
        _ imageAsset: DDBridgeImageAsset,
        device: MTLDevice?,
        context: DirectDrawBackend.Context?,
        commandQueue: MTLCommandQueue?
    ) throws -> DirectDrawBackend.TextureResource {
        guard let device, let context, let imageAssetData = imageAsset.data else {
            throw NSError()
        }

        do {
            Logger.modelGPU.error(
                "[GPUP] imageAssetData = \(imageAssetData)  -  width = \(imageAsset.width)  -  height = \(imageAsset.height)  -  bytesPerPixel = \(imageAsset.bytesPerPixel)"
            )

            var pixelFormat: MTLPixelFormat
            switch imageAsset.bytesPerPixel {
            case 1:
                pixelFormat = .r8Unorm
            case 2:
                pixelFormat = .rg8Unorm
            case 4:
                pixelFormat = .rgba8Unorm
            default:
                pixelFormat = .rgba8Unorm
            }
            let textureDescriptor = MTLTextureDescriptor.texture2DDescriptor(
                pixelFormat: pixelFormat,
                width: Int(imageAsset.width),
                height: Int(imageAsset.height),
                mipmapped: false
            )

            guard let texture = device.makeTexture(descriptor: textureDescriptor) else {
                Logger.modelGPU.error("failed to device.makeTexture")
                throw NSError()
            }
            do {
                try unsafe imageAssetData.bytes.withUnsafeBytes { textureBytes in
                    guard let textureBytesBaseAddress = textureBytes.baseAddress else {
                        throw NSError()
                    }
                    unsafe texture.replace(
                        region: MTLRegionMake2D(0, 0, Int(imageAsset.width), Int(imageAsset.height)),
                        mipmapLevel: 0,
                        withBytes: textureBytesBaseAddress,
                        bytesPerRow: Int(imageAsset.width) * Int(imageAsset.bytesPerPixel)
                    )
                }
            } catch {
                throw NSError()
            }
            let desciptor = makeDirectTextureDescriptorFromMTLTexture(texture)
            let textureResource = try context.makeTextureResource(descriptor: desciptor)
            self.copyTextureToDirectTexture(textureResource.resource, sourceTexture: texture, commandQueue: commandQueue)
            return textureResource
        } catch {
            Logger.modelGPU.error("failed to make texture from image \(error)")
            throw NSError()
        }
    }

    enum MaterialConversionError: Error {
        case unknownMaterialGraphInputOutputDataType(DDBridgeDataType)
        case invalidInput(String, String?)
        case invalidOutput(String, String?)
        case invalidGraph(String)
        case invalidGeometryPropertyDefinition(String)
        case unknownConstantType(DDBridgeConstant)
        case unknownNodeType(DDBridgeNode)
        case invalidTextureForAssetPath(String)
    }

    static fileprivate func sgDataTypeFromInputOutputDataType(_ type: DDBridgeDataType) throws -> SGDataType {
        switch type {
        case .bool: return .bool
        case .int: return .int
        case .int2: return .int2
        case .int3: return .int3
        case .int4: return .int4
        case .float: return .float
        case .color3f: return .color3f
        case .color3h: return .color3h
        case .color4f: return .color4f
        case .color4h: return .color4h
        case .float2: return .float2
        case .float3: return .float3
        case .float4: return .float4
        case .half: return .half
        case .half2: return .half2
        case .half3: return .half3
        case .half4: return .half4
        case .matrix2f: return .matrix2f
        case .matrix3f: return .matrix3f
        case .matrix4f: return .matrix4f
        case .surfaceShader: return .surfaceShader
        case .geometryModifier: return .geometryModifier
        case .string: return .string
        case .token: return .token
        case .asset: return .asset
        @unknown default:
            throw MaterialConversionError.unknownMaterialGraphInputOutputDataType(type)
        }
    }

    static fileprivate func sgNodeFromConstantNode(
        _ constant: DDBridgeConstantContainer,
        name: String
    ) throws -> SGNode {
        let values = constant.constantValues

        switch constant.constant {
        case .bool:
            return try SGNode.create(values[0].number.boolValue, name: name)
        case .uchar:
            return try SGNode.create(values[0].number, type: .uchar, name: name)
        case .int:
            return try SGNode.create(values[0].number, type: .int, name: name)
        case .uint:
            return try SGNode.create(values[0].number, type: .uint, name: name)
        case .half:
            return try SGNode.create(values[0].number, type: .half, name: name)
        case .float:
            return try SGNode.create(values[0].number, type: .float, name: name)
        case .string:
            return try SGNode.create(value: values[0].string as String, type: .string, name: name)
        case .token:
            return try SGNode.create(value: values[0].string as String, type: .token, name: name)
        case .float2:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                ],
                type: .float2,
                name: name
            )
        case .vector3f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .float3,
                name: name
            )
        case .float4:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                ],
                type: .float4,
                name: name
            )
        case .half2:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                ],
                type: .half2,
                name: name
            )
        case .vector3h:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .half3,
                name: name
            )
        case .half4:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                ],
                type: .half4,
                name: name
            )
        case .int2:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                ],
                type: .int2,
                name: name
            )
        case .int3:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .int3,
                name: name
            )
        case .int4:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                ],
                type: .int4,
                name: name
            )
        case .matrix2f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                ],
                type: .matrix2f,
                name: name
            )
        case .matrix3f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                    values[4].number,
                    values[5].number,
                    values[6].number,
                    values[7].number,
                    values[8].number,
                ],
                type: .matrix3f,
                name: name
            )
        case .matrix4f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                    values[4].number,
                    values[5].number,
                    values[6].number,
                    values[7].number,
                    values[8].number,
                    values[9].number,
                    values[10].number,
                    values[11].number,
                    values[12].number,
                    values[13].number,
                    values[14].number,
                    values[15].number,
                ],
                type: .matrix4f,
                name: name
            )
        case .asset:
            return try SGNode.create(value: values[0].string as String, type: .asset, name: name)
        case .timecode:
            return try SGNode.create(values[0].number, type: .timecode, name: name)
        case .quatf:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                ],
                type: .quatf,
                name: name
            )
        case .quath:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                ],
                type: .quath,
                name: name
            )
        case .float3:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .float3,
                name: name
            )
        case .half3:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .half3,
                name: name
            )
        case .point3f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .point3f,
                name: name
            )
        case .point3h:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .point3h,
                name: name
            )
        case .normal3f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .normal3f,
                name: name
            )
        case .normal3h:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .normal3h,
                name: name
            )
        case .color3f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .color3f,
                name: name
            )
        case .color3h:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .color3h,
                name: name
            )
        case .color4f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                ],
                type: .color4f,
                name: name
            )
        case .color4h:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                    values[3].number,
                ],
                type: .color4h,
                name: name
            )
        case .texCoord2h:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                ],
                type: .texCoord2h,
                name: name
            )
        case .texCoord2f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                ],
                type: .texCoord2f,
                name: name
            )
        case .texCoord3h:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .texCoord3h,
                name: name
            )
        case .texCoord3f:
            return try SGNode.create(
                [
                    values[0].number,
                    values[1].number,
                    values[2].number,
                ],
                type: .texCoord3f,
                name: name
            )
        @unknown default:
            throw MaterialConversionError.unknownConstantType(constant.constant)
        }
    }

    static fileprivate func materialFromMaterialGraph(
        _ materialGraph: DDBridgeMaterialGraph,
        context: DirectDrawBackend.Context,
        id: UUID,
        device: MTLDevice,
        nodeDefinitionStore: NodeDefinitionStore,
        textures: [UUID: (DirectDrawBackend.TextureResource, String)],
        textureFromPath: [String: UUID]
    ) throws -> DirectDrawBackend.MaterialResource {
        let name = id.uuidString

        let inputs = try materialGraph.inputs.map { input in
            let type = try sgDataTypeFromInputOutputDataType(input.type)
            guard let input = SGInput.create(name: input.name, type: type) else {
                throw MaterialConversionError.invalidInput(input.name, type.stringValue)
            }
            return input
        }
        let outputs = try materialGraph.outputs.map { output in
            let type = try sgDataTypeFromInputOutputDataType(output.type)
            guard let output = SGOutput.create(name: output.name, type: type) else {
                throw MaterialConversionError.invalidOutput(output.name, type.stringValue)
            }
            return output
        }

        guard let sgGraph = SGGraph.create(name: name, inputs: inputs, outputs: outputs) else {
            throw MaterialConversionError.invalidGraph(name)
        }

        var nodes: [SGNode] = []
        var allNodes: [SGNode] = []

        var index = 0
        for node in materialGraph.nodes {
            let sgNode: SGNode
            var isArgumentOrResultNode = false
            switch node.bridgeNodeType {
            case .builtin:
                sgNode = try SGNode.create(nodeDefName: node.builtin.definition, name: node.builtin.name)
            case .constant:
                sgNode = try sgNodeFromConstantNode(node.constant, name: node.constant.name)
            case .arguments:
                sgNode = sgGraph.argumentsNode
                isArgumentOrResultNode = true
            case .results:
                sgNode = sgGraph.resultsNode
                isArgumentOrResultNode = true
            @unknown default:
                throw MaterialConversionError.unknownNodeType(node)
            }
            if !isArgumentOrResultNode {
                nodes.append(sgNode)
            }
            allNodes.append(sgNode)

            index = index + 1
        }

        try sgGraph.insert(nodes)

        var connections: [AnyObject] = []
        var edgeIndex = 0
        for edge in materialGraph.edges {
            edgeIndex = edgeIndex + 1
            let upstreamNode = allNodes[edge.upstreamNodeIndex]
            let downstreamNode = allNodes[edge.downstreamNodeIndex]
            guard let upstreamOutput = upstreamNode.outputNamed(edge.upstreamOutputName) else {
                throw MaterialConversionError.invalidOutput(edge.upstreamOutputName, nil)
            }
            guard let downstreamInput = downstreamNode.inputNamed(edge.downstreamInputName) else {
                throw MaterialConversionError.invalidInput(edge.downstreamInputName, nil)
            }
            connections.append(contentsOf: [upstreamOutput, downstreamInput])
        }

        try sgGraph.connect(outputInputPairs: connections)

        let configuration = SGMaterialConfiguration()
        for primvar in materialGraph.primvars {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=300636 - figure out how to properly map geomprop definitions
            guard let def = SGGeometryPropertyDefinition.create(name: primvar.name, mappingTo: "UV0") else {
                throw MaterialConversionError.invalidGeometryPropertyDefinition(primvar.name)
            }
            configuration.customGeometryProperties.append(def)
        }
        // Hardcode to correctly map st to uv0
        if let def = SGGeometryPropertyDefinition.create(name: "st", mappingTo: "UV0") {
            configuration.customGeometryProperties.append(def)
        }

        let sgMaterialSource = try ShaderGraphService.createMaterialSource(from: sgGraph, config: configuration)
        let sgMaterial = try ShaderGraphService.materialFromSource(sgMaterialSource, functionConstantValues: .init())
        let options = SGREMaterialCompilationOptions(workingColorSpace: CGColorSpace(name: CGColorSpace.displayP3))
        let mtlLibrary = try ShaderGraphService.createLibrary(from: sgMaterial, device: device, options: options)

        var customSurfaceShaderParameterBindings: [MaterialResource.CustomParameterBinding] = []
        if sgMaterial.hasSurfaceShaderUniforms {
            for property in sgMaterial.customUniformsTypeDescription.properties {
                if case .texture = property.type.mtlDataType {
                    if let assetPath = sgMaterial.textures[property.name],
                        let textureID = textureFromPath[assetPath],
                        let textureResource = textures[textureID]
                    {
                        let textureBinding = MaterialResource.TextureBinding.init(
                            path: assetPath,
                            bindingName: property.name,
                            texture: textureResource.0,
                            pbrInputType: nil
                        )
                        customSurfaceShaderParameterBindings.append(
                            .init(
                                bindingType: .texture(textureBinding),
                                bindingIndex: customSurfaceShaderParameterBindings.count,
                                mtlDataType: .texture,
                                offset: Int(property.type.offset)
                            )
                        )
                    }
                }
            }
        }

        var assetPathToPropertyName: [String: String] = [:]
        for pair in sgMaterial.textures {
            assetPathToPropertyName[pair.value] = pair.key
        }

        let primvars = materialGraph.primvars.map { primvar in
            MaterialResource.Primvar(name: primvar.name, format: MTLAttributeFormat(rawValue: primvar.attributeFormat) ?? .float)
        }

        let sgFunctionDesc = DirectDrawBackend.MaterialResource.ShaderGraphFunctionDesc.init(
            geometryModifierFunctionName: sgMaterial.geometryModifierFunctionName,
            surfaceShaderFunctionName: sgMaterial.surfaceShaderFunctionName,
            hasGeometryModifierUniforms: sgMaterial.hasGeometryModifierUniforms,
            hasSurfaceShaderUniforms: sgMaterial.hasSurfaceShaderUniforms,
            customGeometryModifierParameterBindings: [],
            customSurfaceShaderParameterBindings: customSurfaceShaderParameterBindings
        )

        let isTransparent: Bool = sgMaterial.blending == .transparent

        return context.makeMaterialResource(
            library: mtlLibrary,
            sgFunctionDesc: sgFunctionDesc,
            primvars: primvars,
            isTransparent: isTransparent
        )
    }

    static fileprivate func pbrInputNameToType(_ inputName: String) -> DirectDrawBackend.MaterialResource.TextureBinding.PBRInputType? {
        switch inputName {
        case "diffuseColor":
            return .diffuse
        case "metallic":
            return .metallic
        case "roughness":
            return .roughness
        case "occlusion":
            return .occlusion
        case "normal":
            return .normal
        case "emissive":
            return .emissive
        default:
            return nil
        }
    }

    static fileprivate func convert(_ attributes: [DDBridgeVertexAttributeFormat]?) -> [DDMeshDescriptor.VertexAttributeFormat] {
        guard let attributes else {
            return []
        }

        return attributes.compactMap { a in
            DDMeshDescriptor.VertexAttributeFormat(
                semantic: DDMeshDescriptor.VertexSemantic(rawValue: a.semantic) ?? .invalid,
                format: MTLVertexFormat(rawValue: UInt(a.format)) ?? .invalid,
                layoutIndex: UInt(a.layoutIndex),
                offset: UInt(a.offset)
            )
        }
    }

    static fileprivate func convert(_ vertexLayouts: [DDBridgeVertexLayout]?) -> [DDMeshDescriptor.VertexLayout] {
        guard let vertexLayouts else {
            return []
        }

        return vertexLayouts.compactMap { a in
            DDMeshDescriptor.VertexLayout(
                bufferIndex: UInt(a.bufferIndex),
                bufferOffset: UInt(a.bufferOffset),
                bufferStride: UInt(a.bufferStride)
            )
        }
    }

    static fileprivate func convertToAddMeshRequest(_ request: DDBridgeAddMeshRequest, id: UUID) -> AddMeshRequest {
        AddMeshRequest(
            id: id,
            descriptor:
                DDMeshDescriptor(
                    indexCapacity: Int(request.indexCapacity),
                    indexType: MTLIndexType(rawValue: UInt(request.indexType)) ?? .uint16,
                    vertexBufferCount: Int(request.vertexBufferCount),
                    vertexCapacity: Int(request.vertexCapacity),
                    vertexAttributes: convert(request.vertexAttributes),
                    vertexLayouts: convert(request.vertexLayouts)
                )
        )
    }
    #endif
}

extension Logger {
    fileprivate static let modelGPU = Logger(subsystem: "com.apple.WebKit", category: "model [GPU]")
}

@objc
@implementation
extension DDBridgeReceiver {
    fileprivate let device: MTLDevice

    #if canImport(DirectDrawBackend)
    @nonobjc
    fileprivate let context: DirectDrawBackend.Context
    @nonobjc
    fileprivate let scene: DirectDrawBackend.Scene
    @nonobjc
    fileprivate var meshInstances: [UUID: [DirectDrawBackend.MeshInstance]] = [:]
    @nonobjc
    fileprivate var meshResources: [UUID: DirectDrawBackend.MeshResource] = [:]
    @nonobjc
    fileprivate let camera: Camera
    @nonobjc
    fileprivate var materials: [UUID: DirectDrawBackend.MaterialInstance] = [:]
    @nonobjc
    fileprivate var textures: [UUID: (DirectDrawBackend.TextureResource, String)] = [:]
    @nonobjc
    fileprivate let nodeDefinitionStore: NodeStore
    @nonobjc
    fileprivate let commandQueue: MTLCommandQueue?
    @nonobjc
    fileprivate let captureManager: MTLCaptureManager
    @nonobjc
    fileprivate var textureFromPath: [String: UUID] = [:]
    #endif

    @nonobjc
    private let dispatchSerialQueue: DispatchSerialQueue

    init(
        device: MTLDevice
    ) {
        self.device = device
        self.dispatchSerialQueue = DispatchSerialQueue(label: "DirectDrawXPC", qos: .userInteractive)

        #if canImport(DirectDrawBackend)
        context = DirectDrawBackend.Context.createContext(device: device, dispatchSerialQueue: dispatchSerialQueue)
        scene = context.makeScene()
        commandQueue = device.makeCommandQueue()
        captureManager = MTLCaptureManager.shared()
        camera = context.makeCamera()
        context.setCameraDistance(50.0)
        context.setEnableModelRotation(true)
        nodeDefinitionStore = NodeStore()
        #endif
    }

    @objc(renderWithTexture:)
    func render(with texture: MTLTexture) {
        #if canImport(DirectDrawBackend)

        let captureDescriptor = MTLCaptureDescriptor(from: device)
        do {
            try captureManager.startCapture(with: captureDescriptor)
            print("Capture started at \(captureDescriptor.outputURL?.absoluteString ?? "")")
        } catch {
            Logger.modelGPU.error("failed to start gpu capture \(error)")
        }

        context.update(deltaTime: 1.0 / 60.0)
        camera.renderTarget = .texture(texture)

        context.render(scene: scene, camera: camera)

        captureManager.stopCapture()

        #endif
    }

    @objc(addMesh:identifier:)
    func addMesh(_ descriptor: DDBridgeAddMeshRequest, identifier: UUID) -> Bool {
        #if canImport(DirectDrawBackend)
        self.dispatchSerialQueue.async {
            let meshDescriptor = descriptor.reDescriptor
            do {
                let meshResource = try self.context.makeMeshResource(descriptor: meshDescriptor)
                self.meshResources[identifier] = meshResource
            } catch {
                Logger.modelGPU.error("failed to make mesh resource")
            }
        }
        return true
        #else
        return false
        #endif
    }

    fileprivate func addTextureAsync(_ request: DDBridgeAddTextureRequest, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        do {
            guard let asset = request.imageAsset else {
                Logger.modelGPU.error("GPUP - add texture FAILED bad asset")
                return
            }

            let newTexture = try Helper.makeTextureFromImageAsset(asset, device: device, context: context, commandQueue: commandQueue)
            textures[identifier] = (newTexture, asset.path)
            textureFromPath[asset.path] = identifier
            Logger.modelGPU.error("GPUP - add texture SUCCEEDED")
        } catch {
            Logger.modelGPU.error("GPUP - add texture FAILED")
        }
        #endif
    }

    @objc(addTexture:identifier:)
    func addTexture(_ request: DDBridgeAddTextureRequest, identifier: UUID) -> Bool {
        #if canImport(DirectDrawBackend)
        self.dispatchSerialQueue.async {
            self.addTextureAsync(request, identifier: identifier)
        }
        #endif
        return true
    }

    fileprivate func updateTextureAsync(_ request: DDBridgeUpdateTextureRequest, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        do {
            Logger.modelGPU.error("GPUP - update texture")
            guard let asset = request.imageAsset else {
                return
            }

            let newTexture = try Helper.makeTextureFromImageAsset(asset, device: device, context: context, commandQueue: commandQueue)
            textures[identifier] = (newTexture, asset.path)
            textureFromPath[asset.path] = identifier

            return
        } catch {
            return
        }
        #endif
    }

    @objc(updateTexture:identifier:)
    func updateTexture(_ request: DDBridgeUpdateTextureRequest, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        self.dispatchSerialQueue.async {
            self.updateTextureAsync(request, identifier: identifier)
        }
        #endif
    }

    fileprivate func addMaterialAsync(_ request: DDBridgeAddMaterialRequest, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        do {
            Logger.modelGPU.error("GPUP - add material")
            let materialResource = try Helper.materialFromMaterialGraph(
                request.material,
                context: context,
                id: identifier,
                device: device,
                nodeDefinitionStore: nodeDefinitionStore.nodeDefinitionStore,
                textures: textures,
                textureFromPath: textureFromPath
            )
            let instance = context.makeMaterialInstance(resource: materialResource)
            self.materials[identifier] = instance
        } catch {
            Logger.modelGPU.error("addMaterialAsync FAILED")
            return
        }
        #endif
    }

    @objc(addMaterial:identifier:)
    func addMaterial(_ request: DDBridgeAddMaterialRequest, identifier: UUID) -> Bool {
        #if canImport(DirectDrawBackend)
        self.dispatchSerialQueue.async {
            self.addMaterialAsync(request, identifier: identifier)
        }
        #endif
        return true
    }

    fileprivate func updateMaterialAsync(_ request: DDBridgeUpdateMaterialRequest, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        do {
            Logger.modelGPU.info("GPUP - update material")
            let materialResource = try Helper.materialFromMaterialGraph(
                request.material,
                context: context,
                id: identifier,
                device: device,
                nodeDefinitionStore: nodeDefinitionStore.nodeDefinitionStore,
                textures: textures,
                textureFromPath: textureFromPath
            )

            guard let material = materials[identifier] else {
                return
            }
            material.resource = materialResource
        } catch {
            Logger.modelGPU.error("updateMaterialAsync FAILED")
            return
        }
        #endif
    }

    @objc(updateMaterial:identifier:)
    func updateMaterial(_ request: DDBridgeUpdateMaterialRequest, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        self.dispatchSerialQueue.async {
            self.updateMaterialAsync(request, identifier: identifier)
        }
        #endif
    }

    fileprivate func updateMeshAsync(_ request: DDBridgeUpdateMesh, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        guard let resource = meshResources[identifier] else {
            Logger.modelGPU.error("invalid mesh id")
            return
        }

        let materials = request.materialIds.compactMap { materialId in
            self.materials[materialId]
        }

        let partCount = request.partCount
        resource.resource.partCount = partCount

        for setPart in request.parts {
            resource.resource.setPart(
                index: setPart.partIndex,
                offset: setPart.part.indexOffset,
                count: setPart.part.indexCount,
                topology: MTLPrimitiveType(rawValue: setPart.part.topology) ?? .triangle,
                material: setPart.part.materialIndex,
                boundsMin: setPart.part.boundsMin,
                boundsMax: setPart.part.boundsMax
            )
        }
        for setRenderFlags in request.renderFlags {
            resource.resource.setPart(
                index: setRenderFlags.partIndex,
                renderFlags: setRenderFlags.renderFlags
            )
        }
        for replaceVertices in request.vertices {
            unsafe resource.resource.replaceVertices(index: replaceVertices.bufferIndex) { destination, destinationSize in
                guard destinationSize > 0 else {
                    return
                }
                let sourceSize = replaceVertices.buffer.count
                guard sourceSize <= destinationSize else {
                    Logger.modelGPU.error("Mismatched vertex buffer size, expected \(destinationSize) bytes but got \(sourceSize) bytes")
                    return
                }
                unsafe replaceVertices.buffer.withUnsafeBytes { sourceBuffer in
                    guard let sourceBufferBaseAddress = sourceBuffer.baseAddress else {
                        return
                    }
                    unsafe destination.copyMemory(from: sourceBufferBaseAddress, byteCount: sourceSize)
                }
            }
        }
        if let replaceIndices = request.indices {
            unsafe resource.resource.replaceIndices { destination, destinationSize in
                guard destinationSize > 0 else {
                    return
                }
                let sourceSize = replaceIndices.count
                guard sourceSize <= destinationSize else {
                    Logger.modelGPU.error("Mismatched index buffer size, expected \(destinationSize) bytes but got \(sourceSize) bytes")
                    return
                }
                unsafe replaceIndices.withUnsafeBytes { sourceBuffer in
                    guard let sourceBufferBaseAddress = sourceBuffer.baseAddress else {
                        return
                    }
                    unsafe destination.copyMemory(from: sourceBufferBaseAddress, byteCount: sourceSize)
                }
            }
        }

        let meshInstances: [DirectDrawBackend.MeshInstance]
        if let existingMeshInstances = self.meshInstances[identifier] {
            meshInstances = existingMeshInstances
        } else {
            meshInstances = (0..<resource.resource.partCount)
                .map({ partIndex in
                    context.makeMeshInstance(
                        resource: resource,
                        partIndex: partIndex,
                        material: materials[partIndex],
                        transform: matrix_identity_float4x4,
                        instanceTransforms: []
                    )
                })
            self.meshInstances[identifier] = meshInstances
            self.scene.meshes.append(contentsOf: meshInstances)
        }

        if Helper.isNonZero(matrix: request.transform) {
            for mesh in meshInstances {
                mesh.transform = request.transform
            }
        }
        var instTransforms: [simd_float4x4] = []
        if let chainedTransform = request.instanceTransforms {
            instTransforms.append(chainedTransform.transform)
            while let chainedTransform = chainedTransform.next {
                instTransforms.append(chainedTransform.transform)
            }
        }
        if !instTransforms.isEmpty {
            for mesh in meshInstances {
                mesh.instanceTransforms = instTransforms
            }
        }
        #endif
    }

    @objc(updateMesh:identifier:)
    func updateMesh(_ request: DDBridgeUpdateMesh, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        Logger.modelGPU.error("(pre-call) Material ids \(request.materialIds)")
        self.dispatchSerialQueue.async {
            self.updateMeshAsync(request, identifier: identifier)
        }
        #endif
    }
}
