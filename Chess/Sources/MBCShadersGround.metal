/*
    File:        MBCShadersGround.metal
    Contains:    Metal shader file containing the shader functions used for
                 rendering ground plane below the board with spotlight.
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
 The vertex shader output for ground plane vertex shader
 */
struct VertexOutGround {
    float4 position [[position]];
    float3 worldPostion;
    float2 uv;
    float3 color;
};

vertex VertexOutGround vsChessGround(constant MBCSimpleVertex *vertices [[ buffer(MBCBufferIndexMeshPositions) ]],
                                     constant MBCFrameData &frameData [[ buffer(MBCBufferIndexFrameData) ]],
                                     const device MBCRenderableData *renderableData [[ buffer(MBCBufferIndexRenderableData) ]],
                                     uint vid [[ vertex_id ]],
                                     uint iid [[ instance_id ]]) {
    
    VertexOutGround out;
    
    float2 position = vertices[vid].position;
    float4 vertexPosition = float4(position.x, position.y, 0.f, 1.f);
    
    out.position = frameData.view_projection_matrix * renderableData[iid].model_matrix * vertexPosition;
    
    out.worldPostion = (renderableData[iid].model_matrix * vertexPosition).xyz;
    
    // Covert vertex position coordinate values from [-1, 1] to [0, 1] for uv coordinate sampling
    out.uv = 0.5f * (position + float2(1.f, 1.f));
    
    // Y flip for macOS image coordinates
    out.uv = float2(out.uv.x, 1.f - out.uv.y);
    
    return out;
}

fragment float4 fsChessGround(VertexOutGround in [[ stage_in ]],
                              constant MBCLightData *lightData [[buffer(MBCBufferIndexLightsData)]],
                              texture2d<float> baseColor [[ texture(MBCTextureIndexBaseColor) ]]) {
    
    float4 color = float4(0.f, 0.f, 0.f, 1.f);
    
    constant MBCLightData &spotlight = lightData[MBC_SPOT_LIGHT_INDEX];
    
    const float4 lightColor = float4(spotlight.light_color.xyz, 1.f);
    
    const float3 surfaceNormal = float3(0.f, 1.f, 0.f);
    
    // Distance between light and fragment.
    const float distanceToLight = distance(spotlight.position, in.worldPostion);
    // Direction from fragment to light
    const float3 lightDirection = normalize(spotlight.position - in.worldPostion);
    
    // Cone direction normalized in CPU code
    float spotFactor = dot(lightDirection, -spotlight.spot_cone_direction);
    
    float cosineUmbra = cos(spotlight.spot_umbra_angle);
    if (spotFactor > cosineUmbra) {
        // Fragment is within the outer angle of spotlight.
        const float nDotL = saturate(dot(surfaceNormal, lightDirection));
        float4 sampleColor = baseColor.sample(linearSampler, in.uv);
        
        float t = saturate((spotFactor - cosineUmbra) / (cos(spotlight.spot_penumbra_angle) - cosineUmbra));
        float directionalFalloff = t * t;
        
        float distanceFalloff = clamp((1.f - pow((distanceToLight / spotlight.spot_max_distance_falloff), 2.f)), 0.f, 100.f);
        distanceFalloff *= distanceFalloff;
        
        color += lightColor * distanceFalloff * directionalFalloff * sampleColor * nDotL;
    }
    
    return color;
}
