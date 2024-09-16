/*
    File:        MBCMathUtilities.h
    Contains:    Header for vector, matrix, and quaternion math utility functions useful for 3D graphics rendering.
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
#pragma once

#import <stdlib.h>
#import <simd/simd.h>

/*!
 @abstract Because these are common methods, allow other libraries to overload their implementation.
 */
#define MBC_SIMD_OVERLOAD __attribute__((__overloadable__))

/*!
 @abstract A single-precision quaternion type.
 */
typedef vector_float4 quaternion_float;

/*!
 @abstract Given a uint16_t encoded as a 16-bit float, returns a 32-bit float.
 */
float MBC_SIMD_OVERLOAD float32_from_float16(uint16_t i);

/*!
 @abstract Given a 32-bit float, returns a uint16_t encoded as a 16-bit float.
 */
uint16_t MBC_SIMD_OVERLOAD float16_from_float32(float f);

/*!
 @abstract Returns the number of degrees in the specified number of radians.
 */
float MBC_SIMD_OVERLOAD degrees_from_radians(float radians);

/*!
 @abstract Returns the number of radians in the specified number of degrees.
 */
float MBC_SIMD_OVERLOAD radians_from_degrees(float degrees);

/*!
 @abstract Generates a random float value inside the given range.
 */
inline static float MBC_SIMD_OVERLOAD  random_float(float min, float max) {
    return (((double)random()/RAND_MAX) * (max-min)) + min;
}

/*!
 @abstract Generate a random three-component vector with values between min and max.
 */
vector_float3 MBC_SIMD_OVERLOAD generate_random_vector(float min, float max);

/*!
 @abstract Fast random seed.
 */
void MBC_SIMD_OVERLOAD seedRand(uint32_t seed);

/*!
 @abstract Fast integer random.
 */
int32_t MBC_SIMD_OVERLOAD randi(void);

/*!
 @abstract Fast floating-point random.
 */
float MBC_SIMD_OVERLOAD randf(float x);

/*!
 @abstract Returns a `vector_float3` with the specified xyz components
 */
static inline vector_float3 vector_float3_make(float x, float y, float z)
{
    vector_float3 v;

    v.x = x;
    v.y = y;
    v.z = z;

    return v;
}

/*!
 @abstract Returns a `vector_float4` with the specified xyzw components
 */
static inline vector_float4 vector_float4_make(float x, float y, float z, float w)
{
    vector_float4 v;

    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;

    return v;
}

#define CLAMP(min, max, value) \
    ((value) < (min) ? (min) : (((value) > (max)) ? (max) : (value)))


/*!
 @abstract Returns a vector that is linearly interpolated between the two given vectors.
 */
vector_float3 MBC_SIMD_OVERLOAD vector_lerp(vector_float3 v0, vector_float3 v1, float t);

/*!
 @abstract Returns a vector that is linearly interpolated between the two given vectors.
 */
vector_float4 MBC_SIMD_OVERLOAD vector_lerp(vector_float4 v0, vector_float4 v1, float t);

/*!
 @abstract Converts a unit-norm quaternion into its corresponding rotation matrix.
  */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix3x3_from_quaternion(quaternion_float q);

/*!
 @abstract Constructs a `matrix_float3x3` from three rows of three columns with float values.
 Indices are m<column><row>.
 */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix_make_rows(float m00, float m10, float m20,
                                                    float m01, float m11, float m21,
                                                    float m02, float m12, float m22);

/*!
 @abstract Constructs a `matrix_float4x4` from four rows of four columns with float values.
 Indices are m<column><row>.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_make_rows(float m00, float m10, float m20, float m30,
                                                    float m01, float m11, float m21, float m31,
                                                    float m02, float m12, float m22, float m32,
                                                    float m03, float m13, float m23, float m33);

/*!
 @abstract Constructs a `matrix_float3x3` from 3 `vector_float3` column vectors.
 */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix_make_columns(vector_float3 col0,
                                                       vector_float3 col1,
                                                       vector_float3 col2);

/*!
 @abstract Constructs a `matrix_float4x4` from 4 `vector_float4` column vectors.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_make_columns(vector_float4 col0,
                                                       vector_float4 col1,
                                                       vector_float4 col2,
                                                       vector_float4 col3);

/*!
 @abstract Constructs a rotation matrix from the given angle and axis.
 */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix3x3_rotation(float radians, vector_float3 axis);

/*!
 @abstract Constructs a rotation matrix from the given angle and axis.
 */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix3x3_rotation(float radians, float x, float y, float z);

/*!
 @abstract Constructs a scaling matrix with the specified scaling factors.
 */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix3x3_scale(float x, float y, float z);

/*!
 @abstract Constructs a scaling matrix, using the given vector as an array of scaling factors.
 */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix3x3_scale(vector_float3 s);

/*!
 @abstract Extracts the upper-left 3x3 submatrix of the given 4x4 matrix.
 */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix3x3_upper_left(matrix_float4x4 m);

/*!
 @abstract Returns the inverse of the transpose of the given matrix.
 */
matrix_float3x3 MBC_SIMD_OVERLOAD matrix_inverse_transpose(matrix_float3x3 m);

/*!
 @abstract Constructs a homogeneous rotation matrix from the given angle and axis.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_from_quaternion(quaternion_float q);

/*!
 @abstract Constructs a rotation matrix from the provided angle and axis
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_rotation(float radians, vector_float3 axis);

/*!
 @abstract Constructs a rotation matrix from the given angle and axis.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_rotation(float radians, float x, float y, float z);

/*!
 @abstract Constructs an identity matrix.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_identity(void);

/*!
 @abstract Constructs a scaling matrix with the given scaling factors.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_scale(float sx, float sy, float sz);

/*!
 @abstract Constructs a scaling matrix, using the given vector as an array of scaling factors.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_scale(vector_float3 s);

/*!
 @abstract Constructs a translation matrix that translates by the vector (tx, ty, tz).
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_translation(float tx, float ty, float tz);

/*!
 @abstract Constructs a translation matrix that translates by the vector (t.x, t.y, t.z).
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_translation(vector_float3 t);

/*!
 @abstract Constructs a translation matrix that scales by the vector (s.x, s.y, s.z)
 and translates by the vector (t.x, t.y, t.z).
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix4x4_scale_translation(vector_float3 s, vector_float3 t);

/*!
 @abstract Starting with left-hand world coordinates, constructs a view matrix that is
 positioned at (eyeX, eyeY, eyeZ) and looks toward (centerX, centerY, centerZ),
 with the vector (upX, upY, upZ) pointing up for a left-hand coordinate system.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_look_at_left_hand(float eyeX, float eyeY, float eyeZ,
                                                            float centerX, float centerY, float centerZ,
                                                            float upX, float upY, float upZ);

/*!
 @abstract Starting with left-hand world coordinates, constructs a view matrix that is
 positioned at (eye) and looks toward (target), with the vector (up) pointing
 up for a left-hand coordinate system.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_look_at_left_hand(vector_float3 eye,
                                                            vector_float3 target,
                                                            vector_float3 up);

/*!
 @abstract Starting with right-hand world coordinates, constructs a view matrix that is
 positioned at (eyeX, eyeY, eyeZ) and looks toward (centerX, centerY, centerZ),
 with the vector (upX, upY, upZ) pointing up for a right-hand coordinate system.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_look_at_right_hand(float eyeX, float eyeY, float eyeZ,
                                                             float centerX, float centerY, float centerZ,
                                                             float upX, float upY, float upZ);

/*!
 @abstract Starting with right-hand world coordinates, constructs a view matrix that is
 positioned at (eye) and looks toward (target), with the vector (up) pointing
 up for a right-hand coordinate system.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_look_at_right_hand(vector_float3 eye,
                                                             vector_float3 target,
                                                             vector_float3 up);

/*!
 @abstract Constructs a symmetric orthographic projection matrix, from left-hand eye
 coordinates to left-hand clip coordinates.
 That maps (left, top) to (-1, 1), (right, bottom) to (1, -1), and (nearZ, farZ) to (0, 1).
 The first four arguments are signed eye coordinates.
 nearZ and farZ are absolute distances from the eye to the near and far clip planes.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_ortho_left_hand(float left, float right, float bottom, float top, float nearZ, float farZ);

/*!
 @abstract Constructs a symmetric orthographic projection matrix, from right-hand eye
 coordinates to right-hand clip coordinates.
 That maps (left, top) to (-1, 1), (right, bottom) to (1, -1), and (nearZ, farZ) to (0, 1).
 The first four arguments are signed eye coordinates.
 nearZ and farZ are absolute distances from the eye to the near and far clip planes.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_ortho_right_hand(float left, float right, float bottom, float top, float nearZ, float farZ);

/*!
 @abstract Constructs a symmetric perspective projection matrix, from left-hand eye
 coordinates to left-hand clip coordinates, with a vertical viewing angle of
 fovyRadians, the given aspect ratio, and the given absolute near and far
 z distances from the eye.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_perspective_left_hand(float fovyRadians, float aspect, float nearZ, float farZ);

/*!
 @abstract Constructs a symmetric perspective projection matrix, from right-hand eye
 coordinates to right-hand clip coordinates, with a vertical viewing angle of
 fovyRadians, the given aspect ratio, and the given absolute near and far
 z distances from the eye.
 */
matrix_float4x4  MBC_SIMD_OVERLOAD matrix_perspective_right_hand(float fovyRadians, float aspect, float nearZ, float farZ);

/*!
 @abstract Construct a general frustum projection matrix, from right-hand eye
 coordinates to left-hand clip coordinates.
 The bounds left, right, bottom, and top, define the visible frustum at the near clip plane.
 The first four arguments are signed eye coordinates.
 nearZ and farZ are absolute distances from the eye to the near and far clip planes.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_perspective_frustum_right_hand(float left, float right, float bottom, float top, float nearZ, float farZ);

/*!
 @abstract Returns the inverse of the transpose of the given matrix.
 */
matrix_float4x4 MBC_SIMD_OVERLOAD matrix_inverse_transpose(matrix_float4x4 m);

/*!
 @abstract Constructs an identity quaternion.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_identity(void);

/*!
 @abstract Constructs a quaternion of the form w + xi + yj + zk.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion(float x, float y, float z, float w);

/*!
 @abstract Constructs a quaternion of the form w + v.x*i + v.y*j + v.z*k.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion(vector_float3 v, float w);

/*!
 @abstract Constructs a unit-norm quaternion that represents rotation by the given angle about the axis (x, y, z).
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion(float radians, float x, float y, float z);

/*!
 @abstract Constructs a unit-norm quaternion that represents rotation by the given angle about the specified axis.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion(float radians, vector_float3 axis);

/*!
 @abstract Constructs a unit-norm quaternion from the given matrix.
 The result is undefined if the matrix does not represent a pure rotation.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion(matrix_float3x3 m);

/*!
 @abstract Constructs a unit-norm quaternion from the given matrix.
 The result is undefined if the matrix does not represent a pure rotation.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion(matrix_float4x4 m);

/*!
 @abstract Returns the length of the given quaternion.
 */
float MBC_SIMD_OVERLOAD quaternion_length(quaternion_float q);

/*!
 @abstract Returns the squared length of the given quaternion
 */
float MBC_SIMD_OVERLOAD quaternion_length_squared(quaternion_float q);

/*!
 @abstract Returns the rotation axis of the given unit-norm quaternion.
 */
vector_float3 MBC_SIMD_OVERLOAD quaternion_axis(quaternion_float q);

/*!
 @abstract Returns the rotation angle of the given unit-norm quaternion.
 */
float MBC_SIMD_OVERLOAD quaternion_angle(quaternion_float q);

/*!
 @abstract Returns a quaternion from the given rotation axis and angle, in radians.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_from_axis_angle(vector_float3 axis, float radians);

/*!
 @abstract Returns a quaternion from the given 3x3 rotation matrix.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_from_matrix3x3(matrix_float3x3 m);

/*!
 @abstract Returns a quaternion from the given Euler angle, in radians.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_from_euler(vector_float3 euler);

/*!
 @abstract Returns a unit-norm quaternion.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_normalize(quaternion_float q);

/*!
 @abstract Returns the inverse quaternion of the given quaternion.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_inverse(quaternion_float q);

/*!
 @abstract Returns the conjugate quaternion of the given quaternion.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_conjugate(quaternion_float q);

/*!
 @abstract Returns the product of the two given quaternions.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_multiply(quaternion_float q0, quaternion_float q1);

/*!
 @abstract Returns the quaternion that results from spherically interpolating between the two given quaternions.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_slerp(quaternion_float q0, quaternion_float q1, float t);

/*!
 @abstract Returns the vector that results from rotating the given vector by the given unit-norm quaternion.
 */
vector_float3 MBC_SIMD_OVERLOAD quaternion_rotate_vector(quaternion_float q, vector_float3 v);

/*!
 @abstract Returns the quaternion for the given forward and up vectors for right-hand coordinate systems.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_from_direction_vectors_right_hand(vector_float3 forward, vector_float3 up);

/*!
 @abstract Returns the quaternion for the given forward and up vectors for left-hand coordinate systems.
 */
quaternion_float MBC_SIMD_OVERLOAD quaternion_from_direction_vectors_left_hand(vector_float3 forward, vector_float3 up);

/*!
 @abstract Returns a vector in the +Z direction for the given quaternion.
 */
vector_float3 MBC_SIMD_OVERLOAD forward_direction_vector_from_quaternion(quaternion_float q);

/*!
 @abstract Returns a vector in the +Y direction for the given quaternion (for a left-handed coordinate system,
 negate for a right-hand coordinate system).
 */
vector_float3 MBC_SIMD_OVERLOAD up_direction_vector_from_quaternion(quaternion_float q);

/*!
 @abstract Returns a vector in the +X direction for the given quaternion (for a left-hand coordinate system,
 negate for a right-hand coordinate system).
 */
vector_float3 MBC_SIMD_OVERLOAD right_direction_vector_from_quaternion(quaternion_float q);

