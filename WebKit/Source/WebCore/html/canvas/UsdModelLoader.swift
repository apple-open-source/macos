// Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if canImport(USDDirectDrawIO) && canImport(WebCore_Internal)

internal import WebCore_Internal

internal import Foundation
internal import DirectDrawXPCTypes
import XPC
internal import QuartzCore_Private
internal import ShaderGraph
internal import USDDirectDrawIO
internal import os

final class USDModelLoader: DDRenderDelegate {
    private let renderer: UsdDDRenderer
    private var timer: Timer? = nil
    private let nodeDefinitionStore = NodeDefinitionStore()
    private let objcLoader: WebUSDModelLoader

    @Observable
    final class Host: Identifiable {
        enum State {
            case connecting
            case connected(Connected)
        }
        struct Connected {
            let layerHost = CALayerHost()
            let contextId: UInt32
        }

        let id: UUID
        var state: State

        init(id: UUID, state: State) {
            self.id = id
            self.state = state
        }
    }
    private(set) var hosts: [Host] = []

    @Observable
    final class Mesh: Identifiable {
        enum State {
            case pending
            case created
        }

        let id: UUID
        let descriptor: DDMeshDescriptor
        var state: State

        init(id: UUID, descriptor: DDMeshDescriptor, state: State) {
            self.id = id
            self.descriptor = descriptor
            self.state = state
        }
    }
    private(set) var meshes: [Mesh] = []
    private(set) var primIdToMeshId: [String: UUID] = [:]

    @Observable
    final class Material: Identifiable {
        enum State {
            case pending
            case created
        }

        let id: UUID
        var state: State

        init(id: UUID, state: State) {
            self.id = id
            self.state = state
        }
    }
    private(set) var materials: [Material] = []
    private(set) var primIdToMaterialId: [String: UUID] = [:]
    private(set) var materialIdToTextureIds: [UUID: Set<UUID>] = [:]
    private(set) var assetPathToTextureId: [String: UUID] = [:]

    init(objcInstance: WebUSDModelLoader) {
        objcLoader = objcInstance
        renderer = UsdDDRenderer()
        renderer.delegate = self
    }

    func update(elapsedTime: Double) {
        self.renderer.update(deltaTime: elapsedTime)
    }

    deinit {
        self.renderer.delegate = nil
    }

    func loadModel(from url: URL) {
        renderer.loadModel(from: url)
        renderer.resetTime()
    }

    func loadModel(from data: Data) {
        renderer.loadModel(from: data)
        renderer.resetTime()
    }

    func addHost() {
        let host = Host(id: UUID(), state: .connecting)
        hosts.append(host)
        Logger.modelWCP.info("AddHostedRendererRequest")
    }

    func updateHost(host: Host, size: CGSize, contentsScale: CGFloat) {
        guard case .connected = host.state else {
            return
        }
        objcLoader.updateHostedRenderer(id: host.id, size: size, contentsScale: contentsScale)
    }

    func removeHost(id: UUID) {
        if let host = self.hosts.first(where: { $0.id == id }),
            case .connected = host.state
        {
            Logger.modelWCP.info("removeHost")
        }
        hosts.removeAll(where: { $0.id == id })
    }

    func removeMesh(id: UUID) {
        if let mesh = self.meshes.first(where: { $0.id == id }),
            case .created = mesh.state
        {
            Logger.modelWCP.info("removeMesh")
        }
        meshes.removeAll(where: { $0.id == id })
    }

    func setCameraDistance(_ distance: Float) {
        Logger.modelWCP.info("setCameraDistance")
    }

    func setEnableModelRotation(_ enable: Bool) {
        Logger.modelWCP.info("setEnableModelRotation")
    }

    // MARK: DDRenderDelegate

    func meshCreated(identifier: String) {
        Logger.modelWCP.info("mesh created: \(identifier)")
    }

    func meshUpdated(identifier: String, data: DDMeshData) {
        Logger.modelWCP.info("mesh updated")
        let resourceData = data
        let instanceTransforms = data.instanceTransforms
        let materialPrims = data.materialPrims

        let materialIds: [UUID] = materialPrims.compactMap { primId in
            guard let materialId = primIdToMaterialId[primId] else {
                Logger.modelWCP.info("no material found for \(primId)")
                return nil
            }

            print("[WCP] materialId \(materialId)")
            return materialId
        }
        print("!! Mesh updated - materialIds \(materialIds) -- materialPrims = \(materialPrims)")

        var meshId: UUID? = nil
        if let existingId = primIdToMeshId[identifier], let existingMesh = meshes.first(where: { $0.id == existingId }) {
            if existingMesh.descriptor != resourceData.descriptor {
                // descriptors don't match. remove the mesh and make a new one.
                if case .created = existingMesh.state {
                    Logger.modelWCP.info("RemoveMeshRequest")
                }
                self.meshes.removeAll(where: { $0.id == existingId })
                primIdToMeshId.removeValue(forKey: identifier)
            } else {
                meshId = existingId
            }
        }

        if let meshId {
            Logger.modelWCP.info("update mesh")
            guard let mesh = meshes.first(where: { $0.id == meshId }) else {
                return
            }

            let webRequest = Converter.webUpdateMeshRequestFromUpdateMeshRequest(
                request: UpdateMeshRequest(
                    id: mesh.id,
                    partCount: resourceData.meshParts.count,
                    parts: resourceData.meshParts.enumerated().map({ index, part in .init(partIndex: index, part: part) }),
                    renderFlags: [],
                    vertices: resourceData.vertexData.enumerated().map({ index, data in .init(bufferIndex: index, buffer: data) }),
                    indices: resourceData.indexData,
                    instanceTransforms: instanceTransforms,
                    materialIds: materialIds
                )
            )
            objcLoader.updateMesh(
                webRequest: webRequest
            )
        } else {
            Logger.modelWCP.info("add mesh")
            let mesh = Mesh(id: UUID(), descriptor: resourceData.descriptor, state: .pending)
            meshes.append(mesh)
            primIdToMeshId[identifier] = mesh.id

            let webRequest = Converter.webAddMeshRequestFromAddMeshRequest(
                request: AddMeshRequest(
                    id: mesh.id,
                    descriptor: resourceData.descriptor
                )
            )
            objcLoader.addMesh(
                webRequest: webRequest
            ) { result in
                if result == 1 {
                    mesh.state = .created

                    Logger.modelWCP.info("update! mesh")
                    let webRequest = Converter.webUpdateMeshRequestFromUpdateMeshRequest(
                        request: UpdateMeshRequest(
                            id: mesh.id,
                            partCount: resourceData.meshParts.count,
                            parts: resourceData.meshParts.enumerated().map({ index, part in .init(partIndex: index, part: part) }),
                            renderFlags: [],
                            vertices: resourceData.vertexData.enumerated()
                                .map({
                                    index,
                                    data in .init(bufferIndex: index, buffer: data)
                                }
                                ),
                            indices: resourceData.indexData,
                            instanceTransforms: instanceTransforms,
                            materialIds: materialIds
                        )
                    )
                    self.objcLoader.updateMesh(
                        webRequest: webRequest
                    )
                }
                if result == 0 {
                    Logger.modelWCP.info("Failed to add mesh \(identifier): \(result)")
                    self.meshes.removeAll(where: { $0.id == mesh.id })
                    self.primIdToMeshId.removeValue(forKey: identifier)
                }
            }
        }
    }

    func meshDestroyed(identifier: String) {
        Logger.modelWCP.info("mesh destroyed: \(identifier)")
        guard let meshId = primIdToMeshId[identifier] else {
            return
        }

        if let mesh = meshes.first(where: { $0.id == meshId }) {
            if case .created = mesh.state {
                Logger.modelWCP.info("RemoveMeshRequest")
            }
        }

        meshes.removeAll(where: { $0.id == meshId })
        primIdToMeshId.removeValue(forKey: identifier)
    }

    func materialCreated(identifier: String) {
        Logger.modelWCP.info("Material created: \(identifier)")
    }

    func materialUpdated(identifier: String, data: DDMaterialData) {
        Logger.modelWCP.info("Material changed: \(identifier)")

        // Extract the data we need before entering the Task
        let materialGraph = data.materialGraph
        if let materialId = primIdToMaterialId[identifier] {
            guard let material = materials.first(where: { $0.id == materialId }) else {
                return
            }
            guard case .created = material.state else { return }

            Logger.modelWCP.info("UpdateMaterialRequest")
            let webRequest = Converter.webUpdateMaterialRequestFromUpdateMaterialRequest(
                request: UpdateMaterialRequest(
                    id: materialId,
                    material: materialGraph
                )
            )
            objcLoader.updateMaterial(
                webRequest: webRequest
            )
        } else {
            let material = Material(id: UUID(), state: .pending)
            materials.append(material)
            primIdToMaterialId[identifier] = material.id

            Logger.modelWCP.info("AddMaterialRequest")
            let webRequest = Converter.webAddMaterialRequestFromAddMaterialRequest(
                request: AddMaterialRequest(
                    id: material.id,
                    material: materialGraph
                )
            )
            objcLoader.addMaterial(
                webRequest: webRequest
            )
        }
    }

    func materialDestroyed(identifier: String) {
        Logger.modelWCP.info("Material destroyed: \(identifier)")
        guard let materialId = primIdToMaterialId[identifier] else {
            return
        }

        guard let material = materials.first(where: { $0.id == materialId }) else {
            return
        }
        if case .created = material.state {
            Logger.modelWCP.info("RemoveMaterialRequest")
        }

        materials.removeAll(where: { $0.id == materialId })
        primIdToMaterialId.removeValue(forKey: identifier)
    }

    func textureCreated(identifier: String) {
    }

    func textureUpdated(identifier: String, data: DDTextureData) {
        Logger.modelWCP.info("Texture Updated: \(identifier)")
        print("[WCP] imageAssetData = \(data.asset)")
        let dataAsset = data.asset

        if let textureId = assetPathToTextureId[data.asset.path] {
            Logger.modelWCP.info("UpdateTextureRequest")
            let webRequest = Converter.webUpdateTextureRequestFromUpdateTextureRequest(
                request: UpdateTextureRequest(
                    id: textureId,
                    imageAsset: dataAsset
                )
            )
            objcLoader.updateTexture(
                webRequest: webRequest
            )
        } else {
            let textureId = UUID()
            Logger.modelWCP.info("AddTextureRequest: imageAsset")
            assetPathToTextureId[data.asset.path] = textureId
            let webRequest = Converter.webAddTextureRequestFromAddTextureRequest(
                request: AddTextureRequest(
                    id: textureId,
                    imageAsset: dataAsset
                )
            )
            objcLoader.addTexture(
                webRequest: webRequest
            )
            print(webRequest)
        }
    }

    func textureDestroyed(identifier: String) {
        Logger.modelWCP.info("Texture destroyed: \(identifier)")
        // Assume identifier is aligned with DDImageAsset.path(SdfAsset.resolvedPath)
        if let textureId = assetPathToTextureId[identifier] {
            Logger.modelWCP.info("RemoveTextureRequest: \(textureId)")
        }
    }
}

@objc
@implementation
extension WebDDVertexAttributeFormat {
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
extension WebDDVertexLayout {
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
extension WebAddMeshRequest {
    let indexCapacity: Int32
    let indexType: Int32
    let vertexBufferCount: Int32
    let vertexCapacity: Int32
    let vertexAttributes: [WebDDVertexAttributeFormat]
    let vertexLayouts: [WebDDVertexLayout]
    let identifier: String

    init(
        indexCapacity: Int32,
        indexType: Int32,
        vertexBufferCount: Int32,
        vertexCapacity: Int32,
        vertexAttributes: [WebDDVertexAttributeFormat],
        vertexLayouts: [WebDDVertexLayout],
        identifier: UUID
    ) {
        self.indexCapacity = indexCapacity
        self.indexType = indexType
        self.vertexBufferCount = vertexBufferCount
        self.vertexCapacity = vertexCapacity
        self.vertexAttributes = vertexAttributes
        self.vertexLayouts = vertexLayouts
        self.identifier = identifier.uuidString
    }
}

@objc
@implementation
extension WebDDMeshPart {
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
extension WebSetPart {
    let partIndex: Int
    let part: WebDDMeshPart

    init(index: Int, part: WebDDMeshPart) {
        self.partIndex = index
        self.part = part
    }
}

@objc
@implementation
extension WebSetRenderFlags {
    let partIndex: Int
    let renderFlags: UInt64

    init(index: Int, renderFlags: UInt64) {
        self.partIndex = index
        self.renderFlags = renderFlags
    }
}

@objc
@implementation
extension WebReplaceVertices {
    let bufferIndex: Int
    let buffer: Data

    init(bufferIndex: Int, buffer: Data) {
        self.bufferIndex = bufferIndex
        self.buffer = buffer
    }
}

@objc
@implementation
extension WebChainedFloat4x4 {
    let transform: simd_float4x4
    var next: WebChainedFloat4x4?

    init(
        transform: simd_float4x4
    ) {
        self.transform = transform
    }
}

@objc
@implementation
extension WebDDImageAsset {
    let path: String
    let utType: String
    let data: Data?
    let semantic: WebDDSemantic
    let identifier: String

    init(
        path: String,
        utType: String,
        data: Data?,
        semantic: WebDDSemantic,
        identifier: UUID
    ) {
        self.path = path
        self.utType = utType
        self.data = data
        self.semantic = semantic
        self.identifier = identifier.uuidString
    }
}

@objc
@implementation
extension WebDDAddTextureRequest {
    let imageAsset: WebDDImageAsset

    @objc(initWithImageAsset:)
    init(
        imageAsset: WebDDImageAsset
    ) {
        self.imageAsset = imageAsset
    }
}

@objc
@implementation
extension WebDDUpdateTextureRequest {
    let imageAsset: WebDDImageAsset

    @objc(initWithImageAsset:)
    init(
        imageAsset: WebDDImageAsset
    ) {
        self.imageAsset = imageAsset
    }
}

@objc
@implementation
extension WebDDUpdateMaterialRequest {
    let material: WebDDMaterialGraph

    @objc(initWithMaterial:)
    init(
        material: WebDDMaterialGraph
    ) {
        self.material = material
    }
}

@objc
@implementation
extension WebDDAddMaterialRequest {
    let material: WebDDMaterialGraph

    @objc(initWithMaterial:)
    init(
        material: WebDDMaterialGraph
    ) {
        self.material = material
    }
}

@objc
@implementation
extension WebDDNode {
    let bridgeNodeType: WebDDNodeType
    let builtin: WebDDBuiltin?
    let constant: WebDDConstantContainer?

    init(
        bridgeNodeType: WebDDNodeType,
        builtin: WebDDBuiltin?,
        constant: WebDDConstantContainer?
    ) {
        self.bridgeNodeType = bridgeNodeType
        self.builtin = builtin
        self.constant = constant
    }
}

@objc
@implementation
extension WebDDInputOutput {
    let type: WebDDDataType
    let name: String

    init(
        type: WebDDDataType,
        name: String
    ) {
        self.type = type
        self.name = name
    }
}

@objc
@implementation
extension WebDDPrimvar {
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
extension WebDDConstantContainer {
    let constant: WebDDConstant
    let constantValues: [NSValue]
    let name: String

    init(
        constant: WebDDConstant,
        constantValues: [NSValue],
        name: String
    ) {
        self.constant = constant
        self.constantValues = constantValues
        self.name = name
    }
}

@objc
@implementation
extension WebDDBuiltin {
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
extension WebDDEdge {
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
extension WebDDMaterialGraph {
    let nodes: [WebDDNode]
    let edges: [WebDDEdge]
    let inputs: [WebDDInputOutput]
    let outputs: [WebDDInputOutput]
    let primvars: [WebDDPrimvar]
    let identifier: String

    init(
        nodes: [WebDDNode],
        edges: [WebDDEdge],
        inputs: [WebDDInputOutput],
        outputs: [WebDDInputOutput],
        primvars: [WebDDPrimvar],
        identifier: UUID
    ) {
        self.nodes = nodes
        self.edges = edges
        self.inputs = inputs
        self.outputs = outputs
        self.primvars = primvars
        self.identifier = identifier.uuidString
    }
}

extension Logger {
    fileprivate static let modelWCP = Logger(subsystem: "com.apple.WebKit", category: "model [WCP]")
}

@objc
@implementation
extension WebUpdateMeshRequest {
    let partCount: Int
    let parts: [WebSetPart]?
    let renderFlags: [WebSetRenderFlags]?
    let vertices: [WebReplaceVertices]?
    let indices: Data?
    let transform: simd_float4x4
    var instanceTransforms: WebChainedFloat4x4? // array of float4x4
    let materialIds: [UUID]?
    let identifier: String

    init(
        partCount: Int,
        parts: [WebSetPart]?,
        renderFlags: [WebSetRenderFlags]?,
        vertices: [WebReplaceVertices]?,
        indices: Data?,
        transform: simd_float4x4,
        instanceTransforms: WebChainedFloat4x4?,
        materialIds: [UUID]?,
        identifier: UUID
    ) {
        self.partCount = partCount
        self.parts = parts
        self.renderFlags = renderFlags
        self.vertices = vertices
        self.indices = indices
        self.transform = transform
        self.instanceTransforms = instanceTransforms
        self.materialIds = materialIds
        self.identifier = identifier.uuidString
    }
}

final class Converter {
    static func toWebSetParts(parts: [UpdateMeshRequest.SetPart]?) -> [WebSetPart]? {
        var results: [WebSetPart] = []
        if let p = parts {
            for part in p {
                let p = part.part
                let newMeshPart = WebDDMeshPart(
                    indexOffset: p.indexOffset,
                    indexCount: p.indexCount,
                    topology: p.topology.rawValue,
                    materialIndex: p.materialIndex,
                    boundsMin: p.boundsMin,
                    boundsMax: p.boundsMax
                )
                let newPart = WebSetPart(index: part.partIndex, part: newMeshPart)
                results.append(newPart)
            }
        }
        return results
    }

    static func toWebRenderFlags(flags: [UpdateMeshRequest.SetRenderFlags]?) -> [WebSetRenderFlags]? {
        var results: [WebSetRenderFlags] = []
        guard let flags else {
            return results
        }

        for flag in flags {
            let newFlag = WebSetRenderFlags(index: flag.partIndex, renderFlags: flag.renderFlags)
            results.append(newFlag)
        }

        return results
    }

    static func toWebReplaceVertices(verts: [UpdateMeshRequest.ReplaceVertices]?) -> [WebReplaceVertices]? {
        var results: [WebReplaceVertices] = []
        guard let verts else {
            return results
        }

        for vert in verts {
            let newVert = WebReplaceVertices(bufferIndex: vert.bufferIndex, buffer: vert.buffer)
            results.append(newVert)
        }

        return results
    }

    static func toWebVertexAttributes(attrs: [DDMeshDescriptor.VertexAttributeFormat]?) -> [WebDDVertexAttributeFormat] {
        var results: [WebDDVertexAttributeFormat] = []
        guard let attrs else {
            return results
        }

        for attr in attrs {
            let format = WebDDVertexAttributeFormat(
                semantic: attr.semantic.rawValue,
                format: Int32(attr.format.rawValue),
                layoutIndex: Int32(attr.layoutIndex),
                offset: Int32(attr.offset)
            )
            results.append(format)
        }

        return results
    }

    static func toWebVertexLayouts(layouts: [DDMeshDescriptor.VertexLayout]?) -> [WebDDVertexLayout] {
        var results: [WebDDVertexLayout] = []
        guard let layouts else {
            return results
        }

        for l in layouts {
            let layout = WebDDVertexLayout(
                bufferIndex: Int32(l.bufferIndex),
                bufferOffset: Int32(l.bufferOffset),
                bufferStride: Int32(l.bufferStride)
            )
            results.append(layout)
        }

        return results
    }

    static func webUpdateMeshRequestFromUpdateMeshRequest(request: UpdateMeshRequest) -> WebUpdateMeshRequest {
        var webRequestInstanceTransforms: WebChainedFloat4x4?
        if request.instanceTransforms.count > 0 {
            let countMinusOne = request.instanceTransforms.count - 1
            webRequestInstanceTransforms = WebChainedFloat4x4(transform: request.instanceTransforms[0])
            var instanceTransforms = webRequestInstanceTransforms
            if countMinusOne > 0 {
                for i in 1...countMinusOne {
                    instanceTransforms?.next = WebChainedFloat4x4(transform: request.instanceTransforms[i])
                    instanceTransforms = instanceTransforms?.next
                }
            }
        }

        let webRequest = WebUpdateMeshRequest(
            partCount: request.partCount ?? 0,
            parts: Converter.toWebSetParts(parts: request.parts),
            renderFlags: Converter.toWebRenderFlags(flags: request.renderFlags),
            vertices: Converter.toWebReplaceVertices(verts: request.vertices),
            indices: request.indices,
            transform: request.transform ?? simd_float4x4(),
            instanceTransforms: webRequestInstanceTransforms,
            materialIds: request.materialIds,
            identifier: request.id
        )

        return webRequest
    }

    static func toWebSemantic(_ semantic: DDImageAsset.Semantic) -> WebDDSemantic {
        switch semantic {
        case .color: .color
        case .scalar: .scalar
        case .vector: .vector
        case .unknown: .unknown
        default: .unknown
        }
    }

    static func toWebImageAsset(_ asset: DDImageAsset, id: UUID) -> WebDDImageAsset {
        WebDDImageAsset(path: asset.path, utType: asset.utType, data: asset.data, semantic: toWebSemantic(asset.semantic), identifier: id)
    }

    static func toWebDDNodeType(_ node: DDMaterialGraph.Node) -> WebDDNodeType {
        switch node {
        case .builtin(let nodeDef, let nodeName): .builtin
        case .constant(let constant, let nodeName): .constant
        case .arguments: .arguments
        case .results: .results
        default: .results
        }
    }

    static func toWebDDBuiltin(_ node: DDMaterialGraph.Node) -> WebDDBuiltin? {
        switch node {
        case .builtin(let nodeDef, let nodeName):
            return WebDDBuiltin(definition: nodeDef, name: nodeName)
        case .constant(let constant, let nodeName):
            return nil
        case .arguments:
            return nil
        case .results:
            return nil
        default:
            return nil
        }
    }

    static func constantValues(_ constant: DDMaterialGraph.Node.Constant) -> ([NSValue], WebDDConstant) {
        switch constant {
        case .bool(let bool):
            return ([NSNumber(booleanLiteral: bool)], .bool)
        case .uchar(let uchar):
            return ([NSNumber(value: uchar)], .uchar)
        case .int(let int):
            return ([NSNumber(value: int)], .int)
        case .uint(let uint):
            return ([NSNumber(value: uint)], .uint)
        case .half(let uint16):
            return ([NSNumber(value: uint16)], .half)
        case .float(let float):
            return ([NSNumber(value: float)], .float)
        case .string(let string):
            return ([NSValue(nonretainedObject: string)], .string)
        case .token(let string):
            return ([NSValue(nonretainedObject: string)], .token)
        case .float2(let vector2f):
            return ([NSNumber(value: vector2f.x), NSNumber(value: vector2f.y)], .float2)
        case .vector3f(let vector3f):
            return (
                [
                    NSNumber(value: vector3f.x),
                    NSNumber(value: vector3f.y),
                    NSNumber(value: vector3f.z),
                ], .vector3f
            )
        case .float4(let vector4f):
            return (
                [
                    NSNumber(value: vector4f.x),
                    NSNumber(value: vector4f.y),
                    NSNumber(value: vector4f.z),
                    NSNumber(value: vector4f.w),
                ], .float4
            )
        case .half2(let vector2h):
            return (
                [
                    NSNumber(value: vector2h.x),
                    NSNumber(value: vector2h.y),
                ], .half2
            )
        case .vector3h(let vector3h):
            return (
                [
                    NSNumber(value: vector3h.x),
                    NSNumber(value: vector3h.y),
                    NSNumber(value: vector3h.z),
                ], .vector3h
            )
        case .half4(let vector4h):
            return (
                [
                    NSNumber(value: vector4h.x),
                    NSNumber(value: vector4h.y),
                    NSNumber(value: vector4h.z),
                    NSNumber(value: vector4h.w),
                ], .half4
            )
        case .int2(let vector2i):
            return ([NSNumber(value: vector2i.x), NSNumber(value: vector2i.y)], .int2)
        case .int3(let vector3i):
            return (
                [
                    NSNumber(value: vector3i.x),
                    NSNumber(value: vector3i.y),
                    NSNumber(value: vector3i.z),
                ], .int3
            )
        case .int4(let vector4i):
            return (
                [
                    NSNumber(value: vector4i.x),
                    NSNumber(value: vector4i.y),
                    NSNumber(value: vector4i.z),
                    NSNumber(value: vector4i.w),
                ], .int4
            )
        case .matrix2f(let simd_float2x2):
            return (
                [
                    NSNumber(value: simd_float2x2.columns.0.x),
                    NSNumber(value: simd_float2x2.columns.0.y),
                    NSNumber(value: simd_float2x2.columns.1.x),
                    NSNumber(value: simd_float2x2.columns.1.y),
                ], .matrix2f
            )
        case .matrix3f(let simd_float3x3):
            return (
                [
                    NSNumber(value: simd_float3x3.columns.0.x),
                    NSNumber(value: simd_float3x3.columns.0.y),
                    NSNumber(value: simd_float3x3.columns.0.z),
                    NSNumber(value: simd_float3x3.columns.1.x),
                    NSNumber(value: simd_float3x3.columns.1.y),
                    NSNumber(value: simd_float3x3.columns.1.z),
                    NSNumber(value: simd_float3x3.columns.2.x),
                    NSNumber(value: simd_float3x3.columns.2.y),
                    NSNumber(value: simd_float3x3.columns.2.z),
                ], .matrix3f
            )
        case .matrix4f(let simd_float4x4):
            return (
                [
                    NSNumber(value: simd_float4x4.columns.0.x),
                    NSNumber(value: simd_float4x4.columns.0.y),
                    NSNumber(value: simd_float4x4.columns.0.z),
                    NSNumber(value: simd_float4x4.columns.0.w),
                    NSNumber(value: simd_float4x4.columns.1.x),
                    NSNumber(value: simd_float4x4.columns.1.y),
                    NSNumber(value: simd_float4x4.columns.1.z),
                    NSNumber(value: simd_float4x4.columns.1.w),
                    NSNumber(value: simd_float4x4.columns.2.x),
                    NSNumber(value: simd_float4x4.columns.2.y),
                    NSNumber(value: simd_float4x4.columns.2.z),
                    NSNumber(value: simd_float4x4.columns.2.w),
                    NSNumber(value: simd_float4x4.columns.3.x),
                    NSNumber(value: simd_float4x4.columns.3.y),
                    NSNumber(value: simd_float4x4.columns.3.z),
                    NSNumber(value: simd_float4x4.columns.3.w),
                ], .matrix4f
            )
        case .asset(let assetPath):
            return ([NSValue(nonretainedObject: assetPath)], .asset)
        case .timecode(let timecode):
            return ([NSNumber(value: timecode)], .timecode)
        case .quatf(let quatf):
            return (
                [
                    NSNumber(value: quatf.x),
                    NSNumber(value: quatf.y),
                    NSNumber(value: quatf.z),
                    NSNumber(value: quatf.w),
                ], .quatf
            )
        case .quath(let quath):
            return (
                [
                    NSNumber(value: quath.x),
                    NSNumber(value: quath.y),
                    NSNumber(value: quath.z),
                    NSNumber(value: quath.w),
                ], .quath
            )
        case .float3(let float3):
            return (
                [
                    NSNumber(value: float3.x),
                    NSNumber(value: float3.y),
                    NSNumber(value: float3.z),
                ], .float3
            )
        case .half3(let half3):
            return (
                [
                    NSNumber(value: half3.x),
                    NSNumber(value: half3.y),
                    NSNumber(value: half3.z),
                ], .half3
            )
        case .point3f(let point3f):
            return (
                [
                    NSNumber(value: point3f.x),
                    NSNumber(value: point3f.y),
                    NSNumber(value: point3f.z),
                ], .point3f
            )
        case .point3h(let point3h):
            return (
                [
                    NSNumber(value: point3h.x),
                    NSNumber(value: point3h.y),
                    NSNumber(value: point3h.z),
                ], .point3h
            )
        case .normal3f(let normal3f):
            return (
                [
                    NSNumber(value: normal3f.x),
                    NSNumber(value: normal3f.y),
                    NSNumber(value: normal3f.z),
                ], .normal3f
            )
        case .normal3h(let normal3h):
            return (
                [
                    NSNumber(value: normal3h.x),
                    NSNumber(value: normal3h.y),
                    NSNumber(value: normal3h.z),
                ], .normal3h
            )
        case .color3f(let color3f):
            return (
                [
                    NSNumber(value: color3f.x),
                    NSNumber(value: color3f.y),
                    NSNumber(value: color3f.z),
                ], .color3f
            )
        case .color3h(let color3h):
            return (
                [
                    NSNumber(value: color3h.x),
                    NSNumber(value: color3h.y),
                    NSNumber(value: color3h.z),
                ], .color3h
            )
        case .color4f(let color4f):
            return (
                [
                    NSNumber(value: color4f.x),
                    NSNumber(value: color4f.y),
                    NSNumber(value: color4f.z),
                    NSNumber(value: color4f.w),
                ], .color4f
            )
        case .color4h(let color4h):
            return (
                [
                    NSNumber(value: color4h.x),
                    NSNumber(value: color4h.y),
                    NSNumber(value: color4h.z),
                    NSNumber(value: color4h.w),
                ], .color4h
            )
        case .texCoord2h(let texCoord2h):
            return (
                [
                    NSNumber(value: texCoord2h.x),
                    NSNumber(value: texCoord2h.y),
                ], .texCoord2h
            )
        case .texCoord2f(let texCoord2f):
            return (
                [
                    NSNumber(value: texCoord2f.x),
                    NSNumber(value: texCoord2f.y),
                ], .texCoord2f
            )
        case .texCoord3h(let texCoord3h):
            return (
                [
                    NSNumber(value: texCoord3h.x),
                    NSNumber(value: texCoord3h.y),
                    NSNumber(value: texCoord3h.z),
                ], .texCoord3h
            )
        case .texCoord3f(let texCoord3f):
            return (
                [
                    NSNumber(value: texCoord3f.x),
                    NSNumber(value: texCoord3f.y),
                    NSNumber(value: texCoord3f.z),
                ], .texCoord3f
            )
        default:
            return ([], .asset)
        }
    }

    static func toWebDDConstantContainer(_ node: DDMaterialGraph.Node) -> WebDDConstantContainer? {
        switch node {
        case .builtin(let nodeDef, let nodeName):
            return nil
        case .constant(let constant, let nodeName):
            let converted = constantValues(constant)
            return WebDDConstantContainer(constant: converted.1, constantValues: converted.0, name: nodeName)
        case .arguments:
            return nil
        case .results:
            return nil
        default:
            return nil
        }
    }

    static func toWebNodes(_ nodes: [DDMaterialGraph.Node]) -> [WebDDNode] {
        nodes.compactMap { e in
            WebDDNode(bridgeNodeType: toWebDDNodeType(e), builtin: toWebDDBuiltin(e), constant: toWebDDConstantContainer(e))
        }
    }
    static func toWebEdges(_ edges: [DDMaterialGraph.Edge]) -> [WebDDEdge] {
        edges.compactMap { e in
            WebDDEdge(
                upstreamNodeIndex: e.upstreamNodeIndex,
                downstreamNodeIndex: e.downstreamNodeIndex,
                upstreamOutputName: e.upstreamOutputName,
                downstreamInputName: e.downstreamInputName
            )
        }
    }

    static func toWebDDDataType(_ dataType: DDMaterialGraph.InputOutput.DataType) -> WebDDDataType {
        switch dataType {
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
        default: return .asset
        }
    }

    static func toWebInputOutputs(_ inputs: [DDMaterialGraph.InputOutput]) -> [WebDDInputOutput] {
        inputs.compactMap { e in
            WebDDInputOutput(type: toWebDDDataType(e.type), name: e.name)
        }
    }
    static func toWebPrimvars(_ primvars: [DDMaterialGraph.Primvar]) -> [WebDDPrimvar] {
        primvars.compactMap { e in
            WebDDPrimvar(name: e.name, referencedGeomPropName: e.referencedGeomPropName, attributeFormat: e.attributeFormat.rawValue)
        }
    }

    static func toWebMaterialGraph(_ material: DDMaterialGraph, id: UUID) -> WebDDMaterialGraph {
        WebDDMaterialGraph(
            nodes: toWebNodes(material.nodes),
            edges: toWebEdges(material.edges),
            inputs: toWebInputOutputs(material.inputs),
            outputs: toWebInputOutputs(material.outputs),
            primvars: toWebPrimvars(material.primvars),
            identifier: id
        )
    }

    static func webAddMeshRequestFromAddMeshRequest(request: AddMeshRequest) -> WebAddMeshRequest {
        WebAddMeshRequest(
            indexCapacity: Int32(request.descriptor.indexCapacity),
            indexType: Int32(request.descriptor.indexType.rawValue),
            vertexBufferCount: Int32(request.descriptor.vertexBufferCount),
            vertexCapacity: Int32(request.descriptor.vertexCapacity),
            vertexAttributes: toWebVertexAttributes(attrs: request.descriptor.vertexAttributes),
            vertexLayouts: toWebVertexLayouts(layouts: request.descriptor.vertexLayouts),
            identifier: request.id
        )
    }

    static func webUpdateTextureRequestFromUpdateTextureRequest(request: UpdateTextureRequest) -> WebDDUpdateTextureRequest {
        WebDDUpdateTextureRequest(imageAsset: toWebImageAsset(request.imageAsset, id: request.id))
    }

    static func webAddTextureRequestFromAddTextureRequest(request: AddTextureRequest) -> WebDDAddTextureRequest {
        WebDDAddTextureRequest(imageAsset: toWebImageAsset(request.imageAsset, id: request.id))
    }

    static func webUpdateMaterialRequestFromUpdateMaterialRequest(request: UpdateMaterialRequest) -> WebDDUpdateMaterialRequest {
        WebDDUpdateMaterialRequest(material: toWebMaterialGraph(request.material, id: request.id))
    }

    static func webAddMaterialRequestFromAddMaterialRequest(request: AddMaterialRequest) -> WebDDAddMaterialRequest {
        WebDDAddMaterialRequest(material: toWebMaterialGraph(request.material, id: request.id))
    }
}

@objc
@implementation
extension WebUSDModelLoader {
    @nonobjc
    var renderer: USDModelLoader?
    @nonobjc
    var modelAdded: ((WebAddMeshRequest) -> (Void))?
    @nonobjc
    var modelUpdated: ((WebUpdateMeshRequest) -> (Void))?
    @nonobjc
    var modelAddedCallback: ((Int32) -> (Void))?
    @nonobjc
    var textureAddedCallback: ((WebDDAddTextureRequest) -> (Void))?
    @nonobjc
    var textureUpdatedCallback: ((WebDDUpdateTextureRequest) -> (Void))?
    @nonobjc
    var materialAddedCallback: ((WebDDAddMaterialRequest) -> (Void))?
    @nonobjc
    var materialUpdatedCallback: ((WebDDUpdateMaterialRequest) -> (Void))?

    @nonobjc
    fileprivate var retainedRequests: Set<NSObject> = []

    override init() {
        super.init()

        self.renderer = USDModelLoader(objcInstance: self)
    }

    @objc(
        setCallbacksWithModelAddedCallback:
        modelUpdatedCallback:
        textureAddedCallback:
        textureUpdatedCallback:
        materialAddedCallback:
        materialUpdatedCallback:
    )
    func setCallbacksWithModelAddedCallback(
        _ modelAddedCallback: @escaping ((WebAddMeshRequest) -> (Void)),
        modelUpdatedCallback: @escaping ((WebUpdateMeshRequest) -> (Void)),
        textureAddedCallback: @escaping ((WebDDAddTextureRequest) -> (Void)),
        textureUpdatedCallback: @escaping ((WebDDUpdateTextureRequest) -> (Void)),
        materialAddedCallback: @escaping ((WebDDAddMaterialRequest) -> (Void)),
        materialUpdatedCallback: @escaping ((WebDDUpdateMaterialRequest) -> (Void))
    ) {
        self.modelAdded = modelAddedCallback
        self.modelUpdated = modelUpdatedCallback
        self.textureAddedCallback = textureAddedCallback
        self.textureUpdatedCallback = textureUpdatedCallback
        self.materialAddedCallback = materialAddedCallback
        self.materialUpdatedCallback = materialUpdatedCallback
    }

    @objc
    func loadModel(from url: URL) {
        self.renderer?.loadModel(from: url)
    }

    @objc
    func update(_ deltaTime: Double) {
        self.renderer?.update(elapsedTime: deltaTime)
    }

    @objc
    func requestCompleted(_ request: NSObject) {
        retainedRequests.remove(request)
    }

    fileprivate func updateMesh(webRequest: WebUpdateMeshRequest) {
        print("update! modelUpdated")
        if let modelUpdated {
            retainedRequests.insert(webRequest)
            modelUpdated(webRequest)
        }
    }

    fileprivate func addTexture(webRequest: WebDDAddTextureRequest) {
        print("add! texture added")
        if let textureAddedCallback {
            retainedRequests.insert(webRequest)
            textureAddedCallback(webRequest)
        }
    }

    fileprivate func updateTexture(webRequest: WebDDUpdateTextureRequest) {
        print("update! texture updated")
        if let textureUpdatedCallback {
            retainedRequests.insert(webRequest)
            textureUpdatedCallback(webRequest)
        }
    }

    fileprivate func addMaterial(webRequest: WebDDAddMaterialRequest) {
        print("add! material added")
        if let materialAddedCallback {
            retainedRequests.insert(webRequest)
            materialAddedCallback(webRequest)
        }
    }

    fileprivate func updateMaterial(webRequest: WebDDUpdateMaterialRequest) {
        print("update! material updated")
        if let materialUpdatedCallback {
            retainedRequests.insert(webRequest)
            materialUpdatedCallback(webRequest)
        }
    }

    fileprivate func addMesh(
        webRequest: WebAddMeshRequest,
        callback: @escaping (Int32) -> Void = { result in
            if result == 0 {
                Logger.modelWCP.info("Failure: \(result)")
            }
        }
    ) {
        modelAddedCallback = callback

        if let modelAdded {
            retainedRequests.insert(webRequest)
            modelAdded(webRequest)
        }

        callback(1)
    }

    fileprivate func meshAdded(result: Int32) {
    }

    fileprivate func updateHostedRenderer(id: UUID, size: CGSize, contentsScale: CGFloat) {
        Logger.modelWCP.info("update size scale id")
        return
    }
}

#endif
