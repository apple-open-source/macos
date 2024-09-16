/*
    File:        MBCMetalCamera.mm
    Contains:    Manages the Metal camera to provide view and projection matrices.
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

#import "MBCMetalCamera.h"
#import "MBCMathUtilities.h"
#import "MBCBoardMTLView.h"
#import "MBCDebug.h"
#import "MBCMetalRenderer.h"

const float kStartingElevation = 60.f;
const float kStartingAzimuth = 180.f;
const float kNearClipPlane = 0.1f;
const float kFarClipPlane = 300.f;
const float kDistance = 185.f;
const float kVerticalFieldOfView = 35.f * kDegrees2Radians;

@interface MBCMetalCamera () {
    /*
     @abstract The MTKView size in pixels
     */
    vector_float2 _size;
    
    /*
     @abstract Aspect ratio of the MTKView (width / height)
     */
    float _aspectRatio;
    
    /*!
     @abstract Compute the inverse of the view projection matrix when either of them change.
     Used for the conversion of positions from screen to world coordinates
     */
    matrix_float4x4 _inverseViewProjectionMatrix;
}

// Declare privately as readwrite to use internally
@property (nonatomic, readwrite) matrix_float4x4 projectionMatrix;
@property (nonatomic, readwrite) matrix_float4x4 viewMatrix;
@property (nonatomic, readwrite) matrix_float4x4 viewProjectionMatrix;

@property (nonatomic, readwrite) matrix_float4x4 reflectionViewMatrix;
@property (nonatomic, readwrite) matrix_float4x4 reflectionViewProjectionMatrix;

@end

@implementation MBCMetalCamera

- (instancetype)initWithSize:(vector_float2)size {
    self = [super init];
    if (self) {
        _size = size;
        _aspectRatio = size.x / size.y;
        _viewport = simd_make_float4(0.f, 0.f, size.x, size.y);
        
        _elevation = kStartingElevation;
        _azimuth = kStartingAzimuth;
        
        [self updatePosition];
    }
    return self;
}

- (void)setAzimuth:(float)azimuth {
    _azimuth = azimuth;
    
    [self updatePosition];
}

- (void)setElevation:(float)elevation {
    _elevation = elevation;
    
    [self updatePosition];
}

- (void)updatePosition {
    float cameraY = kDistance * sin(_elevation * kDegrees2Radians);
    float cameraXZ = kDistance * cos(_elevation * kDegrees2Radians);
    float cameraX = cameraXZ * sin(_azimuth * kDegrees2Radians);
    float cameraZ = cameraXZ * -cos(_azimuth * kDegrees2Radians);
    
    _position = simd_make_float3(cameraX, cameraY, cameraZ);

    [self updateViewMatrix];
}

- (void)updateViewMatrix {
    static const vector_float3 kUpVector = { 0.f, 1.f, 0.f};
    static const vector_float3 kZeroVector = { 0.f, 0.f, 0.f };
    
    _viewMatrix = matrix_look_at_right_hand(_position, kZeroVector, kUpVector);

    // Reflection view matrix created from camera positioned below the board
    vector_float3 reflectCameraPosition = simd_make_float3(_position.x, -_position.y, _position.z);
    _reflectionViewMatrix = matrix_look_at_right_hand(reflectCameraPosition, kZeroVector, kUpVector);
    
    [self updateViewProjectionMatrix];
}

- (void)updateSize:(vector_float2)size {
    _size = size;
    _aspectRatio = MAX(1.f, size.x / size.y);
    _viewport = simd_make_float4(0.f, 0.f, size.x, size.y);
    
    [self updateProjectionMatrix];
}

- (void)updateProjectionMatrix {
    _projectionMatrix = matrix_perspective_right_hand(kVerticalFieldOfView, _aspectRatio, kNearClipPlane, kFarClipPlane);
    
    [self updateViewProjectionMatrix];
}

- (void)updateViewProjectionMatrix {
    _viewProjectionMatrix = simd_mul(_projectionMatrix, _viewMatrix);
    _inverseViewProjectionMatrix = simd_inverse(_viewProjectionMatrix);
    
    _reflectionViewProjectionMatrix = simd_mul(_projectionMatrix, _reflectionViewMatrix);
}

- (NSPoint)projectPositionFromModelToScreen:(MBCPosition)inPosition {
    vector_float3 position = { inPosition[0], inPosition[1], inPosition[2] };
    
    // Transform the object coordinates by view projection matrix
    vector_float4 transformed = simd_mul(_viewProjectionMatrix, simd_make_float4(position.x, position.y, position.z, 1.0));

    // Perspective divide to clip space [-1, 1] range for coordinates
    transformed.x /= transformed.w;
    transformed.y /= transformed.w;
    transformed.z /= transformed.w;
    
    // Viewport transformation
    // _viewport is [origin.x, origin.y, width, height]
    // convert [-1, 1] range to [0, 1] range for screen space
    float x = _viewport.x + _viewport.z * (transformed.x * 0.5f + 0.5f);
    
    // subtract for y flip on macOS
    float y = _viewport.y + _viewport.w * (0.5f - transformed.y * 0.5f);

    NSPoint point = {x, y};
    
    return point;
}

- (MBCPosition)unProjectPositionFromScreenToModel:(vector_float2)position fromView:(MBCBoardMTLView *)view {
    simd_float3 worldPos;
    
    // depth from the renderer depth texture
    float z;

    MBCMetalRenderer *renderer = view.renderer;
    z = [renderer readPixel:position];
    if (z < 0.0001) {
        if (MBCDebug::LogMouse()) {
            fprintf(stderr, "No depth buffer for z, redrawing scene\n");
        }
        [view drawNow];
        z = [renderer readPixel:position];
    }
    
    // Create position in clip space coordinates
    simd_float4 input;
    input.x = (position.x / (float)_viewport.z) * 2.0 - 1.0;
    input.y = (position.y / (float)_viewport.w) * 2.0 - 1.0;
    input.z = z;
    input.w = 1.0f;
    
    // Convert from clip space to world space coordinates
    simd_float4 result = simd_mul(_inverseViewProjectionMatrix, input);
    result /= result.w;
    worldPos = simd_make_float3(result.x, result.y, result.z);

    if (MBCDebug::LogMouse()) {
        fprintf(stderr, "Mouse (%.0f,%.0f) @ %5.3f -> (%4.1f,%4.1f,%4.1f)\n",
                position[0], position[1], z, worldPos[0], worldPos[1], worldPos[2]);
    }

    MBCPosition unprojected = {{worldPos.x, worldPos.y, worldPos.z}};

    return unprojected;
}

- (MBCPosition)unProjectPositionFromScreenToModel:(vector_float2)position knownY:(float)knownY {
    // Incoming near point
    simd_float4 inPoint;
    inPoint.x = 2.0 * (position.x / (float)_viewport.z) - 1.0;
    inPoint.y = 2.0 * (position.y / (float)_viewport.w) - 1.0;
    inPoint.z = 1.0f;
    inPoint.w = 1.0f;
    
    simd_float4 inPoint2;
    inPoint2.x = 2.0 * (position.x / (float)_viewport.z) - 1.0;
    inPoint2.y = 2.0 * (position.y / (float)_viewport.w) - 1.0;
    inPoint2.z = 0.0f;
    inPoint2.w = 1.0f;
    
    // World coordinates by multiplying by the inverse of the view projection matrix
    simd_float4 outPoint = simd_mul(_inverseViewProjectionMatrix, inPoint);
    simd_float4 outPoint2 = simd_mul(_inverseViewProjectionMatrix, inPoint2);
    
    // Divide by the fourth component to get back to non-homogeneous coordinates
    outPoint /= outPoint.w;
    outPoint2 /= outPoint2.w;
    
    vector_float3 p1 = outPoint.xyz;
    vector_float3 p0 = outPoint2.xyz;
    
    // Interpolate position by using the knownY along line between p0 and p1
    float yint = (knownY - p1.y) / (p0.y - p1.y);
    vector_float3 worldPos;
    worldPos.x = p1.x + (p0.x - p1.x) * yint;
    worldPos.y = knownY;
    worldPos.z = p1.z + (p0.z - p1.z) * yint;
    
    if (MBCDebug::LogMouse()) {
        fprintf(stderr, "Mouse (%.0f,%.0f) [%5.3f] -> World (%4.1f,%4.1f,%4.1f)\n",
                position[0], position[1], knownY, worldPos[0], worldPos[1], worldPos[2]);
    }

    MBCPosition unprojected = {{worldPos.x, worldPos.y, worldPos.z}};
    return unprojected;
}

@end
