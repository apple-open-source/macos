/*
    File:        MBCDrawStyle.m
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

#import "MBCDrawStyle.h"
#import "MBCShaderTypes.h"

#import <OpenGL/glu.h>
#import <Metal/Metal.h>

@implementation MBCDrawStyle

- (instancetype)init {
    self = [super init];
    if (self) {
        fTexture = 0;
    }
    return self;
}

- (instancetype)initWithTexture:(uint32_t)tex {
    self = [super init];
    if (self) {
        fTexture = tex;
        fDiffuse = 1.0f;
        fSpecular = 0.2f;
        fShininess = 5.0f;
        fAlpha = 1.0f;
        
        fMaterial.roughness = 0.f;
        fMaterial.ambientOcclusion = 1.f;
        fMaterial.metallic = 0.f;
    }
    return self;
}

- (void)unloadTexture {
    if (fTexture) {
        glDeleteTextures(1, &fTexture);
    }
}

- (void)startStyle:(float)alpha {
    GLfloat white_texture_color[4]     =
        {fDiffuse, fDiffuse, fDiffuse, fAlpha*alpha};
    GLfloat emission_color[4]         =
        {0.0f, 0.0f, 0.0f, fAlpha*alpha};
    GLfloat specular_color[4]         =
        {fSpecular, fSpecular, fSpecular, fAlpha*alpha};

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white_texture_color);
    glMaterialfv(GL_FRONT, GL_EMISSION, emission_color);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular_color);
    glMaterialf(GL_FRONT, GL_SHININESS, fShininess);
    glBindTexture(GL_TEXTURE_2D, fTexture);
}

- (MBCSimpleMaterial)materialForPBR {
    return fMaterial;
}

- (void)updateMTLTexture:(id<MTLTexture>)texture {
    fBaseColorTexture = texture;
}

@end
