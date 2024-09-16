/*
    File:        MBCDrawStyle.h
    Contains:    Encapsulates Material settings to use for board or pieces.
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

#import <Foundation/Foundation.h>
#import "MBCShaderTypes.h"

@protocol MTLTexture;

@interface MBCDrawStyle : NSObject {
@public
    /*!
     @abstract The diffuse color component
    */
    float fDiffuse;
    
    /*!
     @abstract The specular color component
    */
    float fSpecular;
    
    /*!
     @abstract The shininess to use to render model
    */
    float fShininess;
    
    /*!
     @abstract The alpha color component to control opacity of model
    */
    float fAlpha;
    
    /*!
     @abstract PBR rendering settings to use when material maps are not available for these attributes.
     */
    MBCSimpleMaterial fMaterial;

@private
    /*!
     @abstract OpenGL texture index
    */
    uint32_t fTexture;
    
    /*!
     @abstract Reference to the base color texture map
    */
    id<MTLTexture> fBaseColorTexture;
}

/*!
 @abstract Texture used for the model's base color
*/
@property (nonatomic, strong) id<MTLTexture> baseColorTexture;

/*!
 @abstract Texture used for the model's normal map
*/
@property (nonatomic, strong) id<MTLTexture> normalMapTexture;

/*!
 @abstract Texture used for the model's roughness and ambient occlusion PBR parameters. Red channel is
 the roughness value and Green is ambient occlusion. Green is unused for now.
*/
@property (nonatomic, strong) id<MTLTexture> roughnessAmbientOcclusionTexture;

/*!
 @abstract init:
 @discussion Default initializer for MBCDrawStyle
*/
- (instancetype)init;

/*!
 @abstract initWithTexture:
 @param tex OpenGL texture id for texture
 @discussion This is the initializer used when creating MBCDrawStyle for OpenGL rendering.
*/
- (instancetype)initWithTexture:(uint32_t)tex;

/*!
 @abstract unloadTexture:
 @discussion Unloads the OpenGL texture, not needed for Metal Rendering
*/
- (void)unloadTexture;

/*!
 @abstract startStyle:
 @param alpha The alpha channel value to use for material.
 @discussion This is used by OpenGL to set the material parameters for currently drawn 3D asset.
*/
- (void)startStyle:(float)alpha;

/*!
 @abstract materialForPBR
 @discussion This method will update the base color texture for the draw style.
*/
- (MBCSimpleMaterial)materialForPBR;

/*!
 @abstract updateMTLTexture:
 @param texture the MTLTexture to use for base color for this style
 @discussion This method will update the base color texture for the draw style.
*/
- (void)updateMTLTexture:(id<MTLTexture>)texture;

@end
