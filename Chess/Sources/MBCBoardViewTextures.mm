/*
	File:		MBCBoardViewTextures.mm
	Contains:	Load OpenGL textures from resources
	Copyright:	© 2002-2011 by Apple Inc., all rights reserved.
	
	Derived from glChess, Copyright © 2002 Robert Ancell and Michael Duelli
	Permission granted to Apple to relicense under the following terms:

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

#import "MBCBoardView.h"
#import "MBCBoardViewDraw.h"
#import "MBCDebug.h"

#import <stdlib.h> 
#import <string.h>
#import <OpenGL/glu.h>
#import <OpenGL/glext.h>
#import <GLUT/glut.h>

GLuint generate_texture(NSString * name, float anisotropy)
{
    GLuint texture_name;
    size_t width    = 128;
    float  scale    = width / 256.0;
    size_t height   = width;
    CGFloat vpos    = 20.0*scale;
    CGFloat hpos    = width*0.5;
    void * 			data 	= calloc(width, height);
    CGContextRef 	bitmap 	= 
        CGBitmapContextCreate(data, width, height, 8, width, NULL, kCGImageAlphaOnly);
    CGRect         everything = {{-10.0*scale,-10.0*scale}, {width+20.0*scale,height+20.0*scale}};
    CGContextClearRect(bitmap, everything);
    CGContextSetAlpha(bitmap, 0.25);
    CGContextSetShouldSubpixelQuantizeFonts(bitmap, false);
    CGContextSelectFont(bitmap, "ArialRoundedMTBold", 280.0*scale, kCGEncodingMacRoman);
    CGContextSetTextDrawingMode(bitmap, kCGTextInvisible);
    CGContextShowTextAtPoint(bitmap, hpos, vpos, [name UTF8String], 1);
    CGPoint textSize = CGContextGetTextPosition(bitmap);
    hpos -= (textSize.x-hpos)*0.5;
    CGContextSetTextDrawingMode(bitmap, kCGTextFill);
    CGContextShowTextAtPoint(bitmap, hpos, vpos, [name UTF8String], 1);
    CGContextRelease(bitmap);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture_name);
    glBindTexture(GL_TEXTURE_2D, texture_name);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    if (anisotropy)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
    
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_ALPHA, width, height, GL_ALPHA,  GL_UNSIGNED_BYTE, data);
    
    free(data);
    
    return texture_name;
}

NSURL * texture_url(NSString * name, NSString * dir)
{
    NSString *      path    = [[NSBundle mainBundle] pathForResource:[name stringByAppendingString:@"@2x"] 
                                                              ofType:@"png" 
                                                         inDirectory:dir];
    if (MBCDebug::Use1xTextures() || ![[NSFileManager defaultManager] fileExistsAtPath:path])
        path    = [[NSBundle mainBundle] pathForResource:name 
                                                  ofType:@"png" 
                                             inDirectory:dir];
   return [NSURL fileURLWithPath:path];
}

GLuint load_texture(NSString * name, NSString * dir, float anisotropy, bool blendX)
{
    NSURL * 			url 	= texture_url(name, dir);
    CGImageSourceRef imgSrc 	= CGImageSourceCreateWithURL((CFURLRef)url, NULL);
    CGImageRef 		img	 	= CGImageSourceCreateImageAtIndex(imgSrc, 0, NULL);
    GLuint			texture_name;
    size_t 			width  	= CGImageGetWidth(img);
    size_t			dWidth	= width*4;
    size_t 			height 	= CGImageGetHeight(img);
    CGRect 			rect 	= {{0, 0}, {width, height}};
    void * 			data 	= calloc(dWidth, height);
    CGColorSpaceRef space 	= CGColorSpaceCreateDeviceRGB();
    CGContextRef 	bitmap 	= 
    CGBitmapContextCreate(data, width, height, 8, dWidth, space,
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
    CGContextDrawImage(bitmap, rect, img);
    CGContextRelease(bitmap);
    CGImageRelease(img);
    CFRelease(imgSrc);
    CGColorSpaceRelease(space);

    if (blendX) {
        size_t blendMax = width / 2;
        for (size_t y=0; y<height; ++y) {
            uint8_t * row = reinterpret_cast<uint8_t *>(data)+y*dWidth;
            for (size_t x=0; x<blendMax; ++x) {
                float   kThisWeight  = 0.5f+x*0.5f/blendMax;
                float   kOtherWeight = 1.0f-kThisWeight;
                for (size_t comp=0; comp<4; ++comp) {
                    uint8_t left                = row[(x<<2)+comp]*kThisWeight+row[((width-x-1)<<2)+comp]*kOtherWeight;
                    uint8_t right               = row[((width-x-1)<<2)+comp]*kThisWeight+row[(x<<2)+comp]*kOtherWeight;
                    row[(x<<2)+comp]            = left;
                    row[((width-x-1)<<2)+comp]  = right;
                }
            }
        }
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture_name);
    glBindTexture(GL_TEXTURE_2D, texture_name);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    if (anisotropy)
       glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA8, width, height, GL_BGRA_EXT,  GL_UNSIGNED_INT_8_8_8_8_REV, data);

    free(data);

    return texture_name;
}

@implementation MBCBoardView ( Textures )

#ifdef CHESS_TUNER

- (void) mergeField:(float)val 
			   into:(NSMutableDictionary *)dict
			  color:(NSString *)color
			  entry:(NSString *)entry
{
	[dict setObject:[NSNumber numberWithFloat:val]
		  forKey:[color stringByAppendingString:entry]];
}
	
- (void) mergeStyleAttr:(MBCDrawStyle *)style
				  color:(NSString *)color
				   into:(NSMutableDictionary *)dict
{
	[self mergeField:style->fDiffuse 
		  into:dict color:color entry:@"Diffuse"];
	[self mergeField:style->fSpecular 
		  into:dict color:color entry:@"Specular"];
	[self mergeField:style->fShininess 
		  into:dict color:color entry:@"Shininess"];
	[self mergeField:style->fAlpha
		  into:dict color:color entry:@"Alpha"];
}

- (NSMutableDictionary *) mergeStyleAttr:(MBCDrawStyle **)style 
									into:(NSDictionary *)dict
{
	NSMutableDictionary * d = [dict mutableCopy];
	[self mergeStyleAttr:style[0] color:@"White" into:d];
	[self mergeStyleAttr:style[1] color:@"Black" into:d];

	return d;
}

- (void) savePieceStyles
{
	NSMutableDictionary * d =
		[self mergeStyleAttr:fPieceDrawStyle into:fPieceAttr];
	[d writeToFile:[[NSBundle mainBundle] pathForResource:@"Piece" 
										  ofType:@"plist"
										  inDirectory:fPieceStyle]
	 atomically:YES];
}

- (void) saveBoardStyles
{
	NSMutableDictionary * d =
		[self mergeStyleAttr:fBoardDrawStyle into:fBoardAttr];
	[self mergeStyleAttr:fBorderDrawStyle color:@"Border" into:d];
	[d setObject:[NSNumber numberWithFloat:fBoardReflectivity]
	   forKey:@"Reflectivity"];	
	[d setObject:[NSNumber numberWithFloat:fLabelIntensity]
	   forKey:@"LabelIntensity"];	
	[d writeToFile:[[NSBundle mainBundle] pathForResource:@"Board" 
										  ofType:@"plist"
										  inDirectory:fBoardStyle]
	 atomically:YES];
}

#endif

- (void) loadField:(float *)field fromAttr:(NSDictionary *)attr
			 color:(NSString *)color entry:(NSString *)entry
{
	NSNumber * val = [attr objectForKey:[color stringByAppendingString:entry]];
	if (val)
		*field = [val floatValue];
}

- (void) loadDrawStyle:(MBCDrawStyle *)drawStyle
			  forColor:(NSString *)color 
				  part:(NSString *)part
				 style:(NSString *)style
				  attr:(NSDictionary *)attr
{
    [drawStyle unloadTexture];
    [drawStyle initWithTexture:
			   load_texture([color stringByAppendingString:part],
							style, fAnisotropy, [part isEqual:@"Piece"])];	
	[self loadField:&drawStyle->fDiffuse 
		  fromAttr:attr color:color entry:@"Diffuse"];
	[self loadField:&drawStyle->fSpecular 
		  fromAttr:attr color:color entry:@"Specular"];
	[self loadField:&drawStyle->fShininess 
		  fromAttr:attr color:color entry:@"Shininess"];
	[self loadField:&drawStyle->fAlpha
		  fromAttr:attr color:color entry:@"Alpha"];
}

/* Load the textures for a color */
- (void) loadColorDrawStyles:(NSString *)cname forColor:(int)color
{
	[self loadDrawStyle:fPieceDrawStyle[color] forColor:cname
		  part:@"Piece" style:fPieceStyle attr:fPieceAttr];
	[self loadDrawStyle:fBoardDrawStyle[color] forColor:cname
		  part:@"Board" style:fBoardStyle attr:fBoardAttr];
}

- (void) loadTextureAttr
{
	NSString * 	p;

    p = [[NSBundle mainBundle] pathForResource:@"Board" 
							   ofType:@"plist" 
							   inDirectory:fBoardStyle];
	[fBoardAttr release];
	fBoardAttr = [[NSDictionary dictionaryWithContentsOfFile:p] retain];
									
    p = [[NSBundle mainBundle] pathForResource:@"Piece" 
							   ofType:@"plist"
							   inDirectory:fPieceStyle];
	[fPieceAttr release];
	fPieceAttr = [[NSDictionary dictionaryWithContentsOfFile:p] retain];
}
	
- (void) loadStyles
{    
	[self loadTextureAttr];
    [self loadColorDrawStyles:@"White" forColor:0];
    [self loadColorDrawStyles:@"Black" forColor:1];
	[self loadDrawStyle:fBorderDrawStyle forColor:@"Border" part:@""
		  style:fBoardStyle attr:fBoardAttr];
	[self loadField:&fBoardReflectivity fromAttr:fBoardAttr 
		  color:@"" entry:@"Reflectivity"];
	[self loadField:&fLabelIntensity fromAttr:fBoardAttr 
		  color:@"" entry:@"LabelIntensity"];
}

- (void) loadStaticTextures
{
    [fSelectedPieceDrawStyle initWithTexture:
		    load_texture(@"selected_piece_texture", nil, fAnisotropy, true)];
	fSelectedPieceDrawStyle->fAlpha	= 0.8f;

    for (char i = '1'; i <= '8'; ++i) 
        fNumberTextures[i - '1'] = 
			generate_texture([NSString stringWithFormat:@"%c", i], fAnisotropy);

    for (char i = 'A'; i <= 'H'; i++) 
        fLetterTextures[i - 'A'] = 
			generate_texture([NSString stringWithFormat:@"%c", i], fAnisotropy);
}

- (void) loadColors
{
	[self loadStaticTextures];
}

@end

// Local Variables:
// mode:ObjC
// End:
