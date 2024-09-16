/*
    File:        MBCShaders.metal
    Contains:    Metal shader file containing the shader functions used for rendering
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

constant float PI_INV = 1.f / M_PI_F;

/*
 The minimum color multiplier value when a fragment is to be in shadow during the forward pass.
 */
constant float kShadowClampedMin = 0.5f;

constant float3 kUpVector = {0.f, 1.f, 0.f};

/*
 The shadow vertex shader output
 */
struct VertexOutShadow {
    float4 position [[position]];
};

/*
 The vertex input into the main vertex shader
 */
struct VertexIn {
    float3 position     [[attribute(MBCVertexAttributePosition)]];
    float2 uv           [[attribute(MBCVertexAttributeTexcoord)]];
    float3 normal       [[attribute(MBCVertexAttributeNormal)]];
    float3 tangent      [[attribute(MBCVertexAttributeTangent)]];
    float3 bitangent    [[attribute(MBCVertexAttributeBitangent)]];
};

/*
 The vertex shader output, which is the input to the main fragment shader
 */
struct VertexOut {
    float4 position [[position]];
    float3 worldPosition;
    float3 worldNormal;
    float3 worldTangent;
    float3 worldBitangent;
    float2 uv;
    float alpha;
    float2 shadowUV;
    float shadowDepth;
};

/*
 Struct to store the parameters for the fragment's surface that are not light dependent.
 These parameters are used for both specular and diffuse lighting.
 */
struct SurfaceParameters {
    float3  viewDirection;
    float3  reflectedVector;
    float3  normal;
    float3  reflectedColor;
    float4  baseColor;
    float3  ambientColor;
    float   nDotV;
    float   nDotUp;
    float   metallic;
    float   roughness;
    float   ambientOcclusion;
};

/*
 Struct to store the parameters that are dependent on each light.
 These parameters are used for both specular and diffuse lighting.
 */
struct LightingParameters {
    float3  irradiatedColor;
    float3  lightDirection;
    float   nDotL;
    float3  halfVector;
    float   nDotH;
    float   lDotH;
};


constexpr sampler nearestSampler(address::repeat,
                                 min_filter::nearest,
                                 mag_filter::nearest,
                                 mip_filter::none);

constexpr sampler shadowSampler(coord::normalized,
                                filter::linear,
                                mip_filter::none,
                                address::clamp_to_edge,
                                compare_func::less);

#pragma mark - PBR Lighting Functions

inline float Fresnel(float dotProduct);
inline float sqr(float a);
float3 computeNormalMap(VertexOut in, texture2d<float> normalMapTexture);
float Distribution(float NdotH, float roughness);
float Geometry(float Ndotv, float alphaG);
float3 computeSpecular(thread SurfaceParameters &surfaceParameters, thread LightingParameters &lightingParameters);
float3 computeDiffuse(thread SurfaceParameters &surfaceParameters, thread LightingParameters &lightingParameters);

inline float Fresnel(float dotProduct) {
    return pow(clamp(1.f - dotProduct, 0.f, 1.f), 5.f);
}

inline float sqr(float a) {
    return a * a;
}

/*
 Will compute the normal value for the fragment using the supplied normal map
 texture and the normal, tangent, and bitangent values from the vertex data.
 */
float3 computeNormalMap(VertexOut in, texture2d<float> normalMapTexture) {
    float4 encodedNormal = normalMapTexture.sample(nearestSampler, float2(in.uv));
    float4 normalMap = float4(normalize(encodedNormal.xyz * 2.f - float3(1.f)), 0.f);
    return float3(normalize(in.worldNormal * normalMap.z + in.worldTangent * normalMap.x + in.worldBitangent * normalMap.y));
}

float Distribution(float NdotH, float roughness) {
    if (roughness >= 1.f) {
        return PI_INV;
    }

    float roughnessSqr = saturate(roughness * roughness);

    float d = (NdotH * roughnessSqr - NdotH) * NdotH + 1.f;
    return roughnessSqr / (M_PI_F * d * d);
}

float Geometry(float Ndotv, float alphaG) {
    float a = alphaG * alphaG;
    float b = Ndotv * Ndotv;
    return (float)(1.f / (Ndotv + sqrt(a + b - a * b)));
}

float3 computeSpecular(thread SurfaceParameters &surfaceParameters, 
                       thread LightingParameters &lightParameters) {
    float specularRoughness = surfaceParameters.roughness * (1.f - surfaceParameters.metallic) + surfaceParameters.metallic;

    float Ds = Distribution(lightParameters.nDotH, specularRoughness);

    float3 Cspec0 = float3(1.f);
    float3 Fs = float3(mix(float3(Cspec0), float3(1.f), Fresnel(lightParameters.lDotH)));
    float alphaG = sqr(specularRoughness * 0.5f + 0.5f);
    float Gs = Geometry(lightParameters.nDotL, alphaG) * Geometry(surfaceParameters.nDotV, alphaG);

    float3 specularOutput = (Ds * Gs * Fs * lightParameters.irradiatedColor) * (1.f + surfaceParameters.metallic * float3(surfaceParameters.baseColor))
    + float3(surfaceParameters.metallic) * lightParameters.irradiatedColor * float3(surfaceParameters.baseColor);

    return specularOutput * surfaceParameters.ambientOcclusion;
}

float3 computeDiffuse(thread SurfaceParameters &surfaceParameters, 
                      thread LightingParameters &lightParameters) {
    float3 diffuseRawValue = float3((PI_INV * surfaceParameters.baseColor) * (1.f - surfaceParameters.metallic));
    return diffuseRawValue * (lightParameters.nDotL * surfaceParameters.ambientOcclusion);
}

constant bool is_using_material_maps [[ function_constant(MBCFunctionConstantIndexUseMaterialMaps) ]];
constant bool is_sampling_reflection [[ function_constant(MBCFunctionConstantIndexSampleReflectionMap) ]];
constant bool is_sampling_irradiance [[ function_constant(MBCFunctionConstantIndexSampleIrradianceMap) ]];
constant bool is_using_board_lighting [[ function_constant(MBCFunctionConstantIndexUseBoardLighting) ]];

SurfaceParameters surfaceParameters(VertexOut in,
                                    constant MBCFrameData &frameData,
                                    texture2d<float> baseColorMap,
                                    texture2d<float> normalMap,
                                    texture2d<float> metallicMap,
                                    texture2d<float> roughnessAOMap) {
    SurfaceParameters parameters;

    parameters.baseColor = baseColorMap.sample(linearSampler, in.uv);
    parameters.normal = normalize(in.worldNormal);
    parameters.viewDirection = normalize(frameData.camera_position - float3(in.worldPosition));
    
    if (is_using_material_maps) {
        // Using image files to sample the following material property values.
        parameters.normal = computeNormalMap(in, normalMap);
        
        float4 roughnessAO = roughnessAOMap.sample(linearSampler, in.uv);
            
        parameters.roughness = max(roughnessAO.x, 0.001f) * 0.8;
            
        parameters.ambientOcclusion = roughnessAO.y;
    }
    parameters.reflectedVector = reflect(-parameters.viewDirection, parameters.normal);
    parameters.nDotV = saturate(dot(parameters.normal, parameters.viewDirection));
    parameters.nDotUp = saturate(dot(parameters.normal, kUpVector));

    return parameters;
}

LightingParameters lightingParameters(constant MBCLightData &lightData,
                                      thread SurfaceParameters &surfaceParameters, 
                                      float3 lightPosition,
                                      float3 fragmentPosition) {
    LightingParameters parameters;
    parameters.irradiatedColor = lightData.specular_color;
    parameters.lightDirection = lightData.direction_is_position ? normalize(lightPosition) : normalize(lightPosition - fragmentPosition);
    parameters.nDotL = saturate(dot(surfaceParameters.normal, parameters.lightDirection));
    parameters.halfVector = normalize(parameters.lightDirection + surfaceParameters.viewDirection);
    parameters.nDotH = saturate(dot(surfaceParameters.normal, parameters.halfVector));
    parameters.lDotH = saturate(dot(parameters.lightDirection, parameters.halfVector));
    
    return parameters;
}

inline half evaluateShadowMap(depth2d<float> shadowMap, float2 shadowUV, float shadowDepth) {
    // Will sample fragments around current fragment to find average value. This
    // will produce a softer edged shadow from the shadow map.
    const int fragmentOffset = 1;
    half sampleShadow = 0.h;
    for (int horizontalOffset = -fragmentOffset; horizontalOffset <= fragmentOffset; ++horizontalOffset) {
        for (int verticalOffset = -fragmentOffset; verticalOffset <= fragmentOffset; ++verticalOffset) {
            // Compare the depth value in the shadow map to the depth value of this fragment in the directional lights's
            // frame of reference.  If the sample is occluded, it will be zero and thus will clamp to minimum.
            // Using a minimum greater than 0.f so the shadow is not too dark (black).
            float sampled = shadowMap.sample_compare(shadowSampler, shadowUV, shadowDepth, int2(horizontalOffset, verticalOffset));
            sampleShadow += clamp(sampled, kShadowClampedMin, 1.f);
        }
    }
    // ie 2 - (-2) has 5 samples, which is 2 * 2 + 1. Will square this
    // since have a double loop and thus iterating 5 * 5 = 25 times.
    half count = pow(2.f * (float)fragmentOffset + 1.f, 2.f);
    sampleShadow /= count;
    
    return sampleShadow;
}

#pragma mark - Shadow Vertex Shader

/*
 The shadow pass vertex shader for computing vertex positions relative to the camera from main light position.
 The main purpose of this shader is to create a depth map from a camera placed at main light position. This
 depth map will be used to generate the shadows in the forward pass rendering.
 */
vertex VertexOutShadow vsChessShadows(const device MBCShadowVertex *positions [[buffer(MBCBufferIndexMeshPositions)]],
                                      constant MBCFrameData &frameData [[buffer(MBCBufferIndexFrameData)]],
                                      const device MBCRenderableData *renderableData [[buffer(MBCBufferIndexRenderableData)]],
                                      uint vid [[vertex_id]],
                                      uint iid [[instance_id]]) {

    VertexOutShadow out;
    float4 position = float4(positions[vid].position, 1.f);
    
    // Convert the vertex position to world space (model_matrix), then apply the shadow view and projection
    // matrix to place the position in the space from the perspective of a camera placed at the position
    // of the main directional light that will be casting shadows.
    out.position = frameData.shadow_vp_matrix * renderableData[iid].model_matrix * position;
    
    return out;
}

#pragma mark - PBR Shaders

/*
 The main vertex shader for rendering renderable mesh instances in the scene
 */
vertex VertexOut vsChess(VertexIn in [[stage_in]],
                         constant MBCFrameData &frameData [[buffer(MBCBufferIndexFrameData)]],
                         const device MBCRenderableData *renderableData [[buffer(MBCBufferIndexRenderableData)]],
                         uint iid [[instance_id]]) {
    
    float4 modelPosition = float4(in.position, 1.f);
    
    // Lighting calculations being performed in world space, do not need view matrix
    float4x4 objToWorld = renderableData[iid].model_matrix;
    
    // Normal matrix by removing translation
    float3x3 normalMatrix = float3x3(objToWorld.columns[0].xyz,
                                     objToWorld.columns[1].xyz,
                                     objToWorld.columns[2].xyz);
    
    // Transform the vertex position to be in the clip space of the shadow map depth texture.
    float3 shadowCoord = (frameData.shadow_vp_texture_matrix * objToWorld * modelPosition).xyz;
    
    VertexOut out {
        .position = frameData.view_projection_matrix * renderableData[iid].model_matrix * modelPosition,
        .worldPosition = (objToWorld * modelPosition).xyz,
        .worldNormal = normalMatrix * in.normal,
        .worldTangent = normalMatrix * in.tangent,
        .worldBitangent = normalMatrix * in.bitangent,
        .uv = in.uv,
        .alpha = renderableData[iid].alpha,
        .shadowUV = shadowCoord.xy,
        .shadowDepth = shadowCoord.z
    };
    
    return out;
}

/*
 The main fragment shader for rendering renderable PBR mesh instances in the scene
 */
fragment float4 fsChess(VertexOut in [[stage_in]],
                        constant MBCFrameData &frameData [[buffer(MBCBufferIndexFrameData)]],
                        constant MBCLightData *lightData [[buffer(MBCBufferIndexLightsData)]],
                        texture2d<float> baseColor [[texture(MBCTextureIndexBaseColor)]],
                        constant MBCSimpleMaterial &material [[buffer(MBCBufferIndexMaterialData)]],
                        texturecube<float> irradianceMap [[texture(MBCTextureIndexIrradianceMap)]],
                        texture2d<float> normalMap [[texture(MBCTextureIndexNormal), function_constant(is_using_material_maps)]],
                        texture2d<float> metallicMap [[texture(MBCTextureIndexMetallic), function_constant(is_using_material_maps)]],
                        texture2d<float> roughnessAOMap [[texture(MBCTextureIndexRoughnessAO), function_constant(is_using_material_maps)]],
                        depth2d<float> shadowMap [[texture(MBCTextureIndexShadow)]],
                        depth2d<float> reflectionMap [[texture(MBCTextureIndexReflection), function_constant(is_sampling_reflection)]]) {
    
    SurfaceParameters surfaceParams = surfaceParameters(in,
                                                        frameData,
                                                        baseColor,
                                                        normalMap,
                                                        metallicMap,
                                                        roughnessAOMap);
    
    // No textures for these material properties, use values from material properties
    surfaceParams.metallic = material.metallic;
    if (!is_using_material_maps) {
        surfaceParams.roughness = material.roughness;
        surfaceParams.ambientOcclusion = material.ambientOcclusion;
    }
    
    // Irradiance Map
    float4 irradianceDiffuse = float4(0.f);
    if (is_sampling_irradiance) {
        irradianceDiffuse = irradianceMap.sample(linearSampler, surfaceParams.normal);
        irradianceDiffuse *= surfaceParams.baseColor;
    }
    
    // Main Directional Light
    constant MBCLightData &mainLight = lightData[MBC_MAIN_LIGHT_INDEX];
    LightingParameters lightingParams = lightingParameters(mainLight, surfaceParams, frameData.light_positions[0], in.worldPosition);

    float mainIntensity = mainLight.light_intensity;
    float specularIntensity = 1.f;
    if (is_using_board_lighting) {
        specularIntensity = frameData.main_light_specular_intensity;
    }
    
    float4 color = float4(specularIntensity * computeSpecular(surfaceParams, lightingParams) +
                          mainIntensity * computeDiffuse(surfaceParams, lightingParams) +
                          irradianceDiffuse.xyz, 1.f);
    
    // Reflection Map
    if (is_sampling_reflection) {
        // Multiply by 2 since texture is half width and height
        float width = (float)reflectionMap.get_width() * 2.f;
        float height = (float)reflectionMap.get_height() * 2.f;
        float x = in.position.x / width;
        float y = in.position.y / height;
        float2 reflectUV = float2(x, 1.f - y);
        
        // Only reflect from positive Y surface, thus multiply by normal dot up vector.
        float4 reflectColor = reflectionMap.sample(linearSampler, reflectUV) * surfaceParams.nDotUp;
        
         // mix returns the linear blend of reflectColor and color (x, y, a) implemented as:
         //   x + (y – x) * a
        color = mix(reflectColor, color, 0.85f);
    }

    // Sampling shadows before fill lights so shadows do not darken the fill lighting.
    half sampleShadow = evaluateShadowMap(shadowMap, in.shadowUV, in.shadowDepth);
    if (is_using_board_lighting) {
        if (dot(kUpVector, surfaceParams.normal) <= 0.f) {
            // Just uniformly darken the vertical edge (of the board).
            sampleShadow = 0.5f;
        }
    }
    color *= sampleShadow;
    
    // Fill Lighting
    for (int i = MBC_FILL_LIGHT_START_INDEX; i < frameData.light_count; ++i) {
        constant MBCLightData &fillLight = lightData[i];
        LightingParameters fillLightParams = lightingParameters(fillLight, surfaceParams, frameData.light_positions[i], in.worldPosition);
        
        float fillIntensity = fillLight.light_intensity;
        float3 specularComponent = 0.f;
        if (!is_using_board_lighting) {
            specularComponent = computeSpecular(surfaceParams, fillLightParams);
        }
        float4 fillColor = float4(specularComponent +
                                  fillIntensity * computeDiffuse(surfaceParams, fillLightParams), 1.f);
        color += fillColor;
    }

    color.w = in.alpha;
    
    return color;
}
