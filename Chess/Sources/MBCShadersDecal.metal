/*
    File:        MBCShadersDecal.metal
    Contains:    Metal shaders for rendering chess board surface decal graphics.
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

#include <metal_stdlib>

#include "MBCShaderTypes.h"

using namespace metal;

/*
 The vertex shader output for decal drawing
 */
struct VertexOutDecal {
    float4 position [[position]];
    float2 uv;
    float3 color;
    float3 worldNormal;
    float3 worldTangent;
    float3 worldBitangent;
};

vertex VertexOutDecal vsChessDecal(constant MBCSimpleVertex *vertices [[ buffer(MBCBufferIndexMeshPositions) ]],
                                   constant MBCFrameData &frameData [[ buffer(MBCBufferIndexFrameData) ]],
                                   const device MBCDecalRenderableData *renderableData [[ buffer(MBCBufferIndexRenderableData) ]],
                                   uint vid [[ vertex_id ]],
                                   uint iid [[ instance_id ]]) {
    VertexOutDecal out;
    
    // Scale the vertex XY to set the size of the quad.
    float2 position = vertices[vid].position;
    float vertexScale = renderableData[iid].quad_vertex_scale;
    float4 vertexPosition = float4(position.x * vertexScale, position.y * vertexScale, 0.f, 1.f);
    
    out.position = frameData.view_projection_matrix * renderableData[iid].model_matrix * vertexPosition;
    
    // Covert vertex position coordinate values from [-1, 1] to [0, 1] for uv coordinate sampling
    out.uv = 0.5f * (position + float2(1.f, 1.f));
    
    // Y flip for macOS image coordinates
    out.uv = float2(out.uv.x, 1.f - out.uv.y);
    
    // Digits are in a 4x4 grid with 0 in bottom left (0, 0). Thus, need to scale and offset the
    // uv from size 0->1 to size 0->0.25 and origin offset for digit value to draw.
    out.uv = saturate((out.uv * renderableData[iid].uv_scale) + renderableData[iid].uv_origin);
    
    out.color = renderableData[iid].color;
    
    out.worldNormal = float3(0.f, 1.f, 0.f);
    out.worldTangent = float3(1.f, 0.f, 0.f);
    out.worldBitangent = float3(0.f, 0.f, 1.f);
    
    return out;
}

fragment float4 fsChessDecal(VertexOutDecal in [[ stage_in ]],
                             constant MBCLightData *lightData [[buffer(MBCBufferIndexLightsData)]],
                             texture2d<float> baseColor [[ texture(MBCTextureIndexBaseColor) ]],
                             texture2d<float> normalMapTexture [[ texture(MBCTextureIndexNormal)]]) {
    float4 color = baseColor.sample(linearSampler, in.uv);
    color.xyz = color.xyz * color.w * in.color;
    
    if (!is_null_texture(normalMapTexture)) {
        // Sample normal map, then change from [0, 1] range to [-1, 1] range.
        float4 encodedNormal = normalMapTexture.sample(linearSampler, float2(in.uv));
        encodedNormal = float4(normalize(encodedNormal.xyz * 2.f - float3(1.f)), 0.f);
        
        // Apply normal data from map to the wold normal, tangent, and bitangent.
        float3 normal = float3(normalize(in.worldNormal * encodedNormal.z + in.worldTangent * encodedNormal.x + in.worldBitangent * encodedNormal.y));
        float nDotL = dot(normal, lightData[0].normalized_direction);
        color = nDotL * color;
    }
    
    return color;
}
