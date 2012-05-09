/*
	File:		MBCBoardViewTextures.mm
	Contains:	Load OpenGL textures from resources
	Version:	1.0
	Copyright:	© 2002-2012 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCBoardView.h"
#import "MBCBoardViewDraw.h"

#import <stdlib.h> 
#import <string.h>
#import <OpenGL/glu.h>
#import <OpenGL/glext.h>
#import <GLUT/glut.h>


NSURL * texture_url(NSString * name, NSString * dir)
{
    NSString *      path    = [[NSBundle mainBundle] pathForResource:[name stringByAppendingString:@"@2x"] 
                                                              ofType:@"png" 
                                                         inDirectory:dir];
    if (![[NSFileManager defaultManager] fileExistsAtPath:path])
        path    = [[NSBundle mainBundle] pathForResource:name 
                                                  ofType:@"png" 
                                             inDirectory:dir];
    return [NSURL fileURLWithPath:path];
}

GLuint load_texture(NSString * name, NSString * dir, BOOL mono, float anisotropy, bool blendX)
{
    NSURL * 			url 	= texture_url(name, dir);
   CGImageSourceRef imgSrc 	= CGImageSourceCreateWithURL((CFURLRef)url, NULL);
   CGImageRef 		img	 	= CGImageSourceCreateImageAtIndex(imgSrc, 0, NULL);
   GLuint			texture_name;
   size_t 			width  	= CGImageGetWidth(img);
   size_t			dWidth	= mono ? width : width*4;
   size_t 			height 	= CGImageGetHeight(img);
   CGRect 			rect 	= {{0, 0}, {width, height}};
   void * 			data 	= calloc(dWidth, height);
   CGColorSpaceRef 	space 	= mono ? NULL : CGColorSpaceCreateDeviceRGB();
   CGContextRef 	bitmap 	= 
	CGBitmapContextCreate(data, width, height, 8, dWidth, space,
		 mono ? kCGImageAlphaOnly
		 : kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
   CGContextDrawImage(bitmap, rect, img);
   CGContextRelease(bitmap);
   CGImageRelease(img);
   CFRelease(imgSrc);
   if (!mono)
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
   
   if (mono)
	   gluBuild2DMipmaps(GL_TEXTURE_2D, GL_ALPHA, width, height, GL_ALPHA,  GL_UNSIGNED_BYTE, data);
   else
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
							style, FALSE, fAnisotropy, [part isEqual:@"Piece"])];	
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
		    load_texture(@"selected_piece_texture", nil, FALSE, fAnisotropy, true)];
	fSelectedPieceDrawStyle->fAlpha	= 0.8f;

    for (char i = '1'; i <= '8'; ++i) 
        fNumberTextures[i - '1'] = 
			load_texture([NSString stringWithFormat:@"%c", i], nil, TRUE, fAnisotropy, false);

    for (char i = 'a'; i <= 'h'; i++) 
        fLetterTextures[i - 'a'] = 
			load_texture([NSString stringWithFormat:@"%c", i], nil, TRUE, fAnisotropy, false);
}

- (void) loadColors
{
	[self loadStaticTextures];
}

@end

// Local Variables:
// mode:ObjC
// End:
