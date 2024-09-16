/*
    File:        MBCShadersArrow.metal
    Contains:    Metal shader file containing the shader functions used
                 for rendering board arrows
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

struct VertexOutArrow
{
    float4 position [[position]];
    float2 uv;
    float4 color;
    float animationOffset;
    float normalizedYPosition;
};

vertex VertexOutArrow vsChessArrow(constant MBCArrowVertex *vertices [[ buffer(MBCBufferIndexMeshPositions) ]],
                                   constant int *indices [[ buffer(MBCBufferIndexTriangleIndices) ]],
                                   constant MBCFrameData &frameData [[ buffer(MBCBufferIndexFrameData) ]],
                                   const device MBCArrowRenderableData *renderableData [[ buffer(MBCBufferIndexRenderableData) ]],
                                   uint vid [[ vertex_id ]],
                                   uint iid [[ instance_id ]]) {
    VertexOutArrow out;
    
    int index = indices[vid];
    
    float2 position = vertices[index].position;
    float2 uv = vertices[index].uv;
    
    // Multiply by model matrix to set the scale, rotation, and position to position arrow on board.
    // The arrow is on XZ plane, which is why putting position.y in Z component below.
    float4 vertexPosition = float4(position.x, 0.f, position.y, 1.f);
    out.position = frameData.view_projection_matrix * renderableData[iid].model_matrix * vertexPosition;

    out.uv = uv;
    out.uv.y = 1.f - uv.y;
    out.color = renderableData[iid].color;

    out.animationOffset = renderableData[iid].animation_offset;
    
    // Convert vertex y position to normalized value from 0 to 1
    float halfLength = renderableData[iid].length * 0.5f;
    out.normalizedYPosition = 1.f - (position.y + halfLength) / renderableData[iid].length;;
    
    return out;
}

fragment float4 fsChessHintArrow(VertexOutArrow in [[ stage_in ]],
                                 texture2d<float> baseColor [[ texture(MBCTextureIndexBaseColor) ]]) {
    // Apply arrow color to the grayscale arrow texture
    float4 color = baseColor.sample(linearSampler, in.uv);
    color.xyz = color.xyz + in.color.xyz * color.w;
    
    // Compute sin value with animation offset. Value is converted from
    // -1 to 1 range to 0 to 1 range.
    float angle = in.normalizedYPosition * M_PI_F + in.animationOffset;
    float sinValue = (sin(angle) + 1) * 0.5f;
    
    float3 halfColor = 0.5f * color.xyz;
    color.xyz = halfColor + (sinValue * halfColor);
    
    return color;
}

fragment float4 fsChessLastMoveArrow(VertexOutArrow in [[ stage_in ]],
                                     texture2d<float> baseColor [[ texture(MBCTextureIndexBaseColor) ]]) {
    // Apply arrow color to the grayscale arrow texture, no animation for last moves.
    float4 color = baseColor.sample(linearSampler, in.uv);
    color += (in.color * in.color.w * color.w);
    color.w *= in.color.w;
    
    return color;
}
