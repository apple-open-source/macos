/*
	File:		MBCBoardViewTextures.mm
	Contains:	Load OpenGL textures from resources
	Version:	1.0
	Copyright:	Â© 2002-2008 by Apple Computer, Inc., all rights reserved.
	
	Derived from glChess, Copyright © 2002 Robert Ancell and Michael Duelli
	Permission granted to Apple to relicense under the following terms:

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardViewTextures.mm,v $
		Revision 1.16  2008/10/24 20:06:17  neerache
		<rdar://problem/3710028> ER: Chessboard anti-aliasing
		
		Revision 1.15  2008/10/24 01:17:14  neerache
		<rdar://problem/5973744> Chess Needs To Move from SGI Format Images to PNG or JPEG
		
		Revision 1.14  2007/03/01 20:58:48  neerache
		Plug texture leaks <rdar://problem/3987435>
		
		Revision 1.13  2006/10/09 21:43:50  neerache
		UTF-8 cleanliness is next to godliness <rdar://problems/4325719>
		
		Revision 1.12  2004/09/02 11:07:53  neerache
		Use anisotropic textures
		
		Revision 1.11  2004/07/10 04:53:29  neerache
		Tweak visuals
		
		Revision 1.10  2003/11/06 23:30:51  neerache
		Adjust wording as suggested by Joyce Chow
		
		Revision 1.9  2003/10/29 22:39:31  neerache
		Add tools & clean up copyright references for release
		
		Revision 1.8  2003/08/01 23:53:19  neerache
		Get rid of erroneous use of GL_SRC_COLOR (RADAR 3343477)
		
		Revision 1.7  2003/06/05 08:31:26  neerache
		Added Tuner
		
		Revision 1.6  2003/06/04 23:14:05  neerache
		Neater manipulation widget; remove obsolete graphics options
		
		Revision 1.5  2003/06/02 04:21:40  neerache
		Start implementing drawing styles for board elements
		
		Revision 1.4  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.3  2003/05/05 23:52:05  neerache
		Experimental switch to mipmaps
		
		Revision 1.2  2002/10/15 22:49:40  neeri
		Add support for texture styles
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import "MBCBoardView.h"
#import "MBCBoardViewDraw.h"

#import <stdlib.h> 
#import <string.h>
#import <OpenGL/glu.h>
#import <OpenGL/glext.h>
#import <GLUT/glut.h>

GLuint load_texture(NSString * name, NSString * dir, BOOL mono, float anisotropy)
{
   name = [[NSBundle mainBundle] pathForResource:name 
							   ofType:@"png" 
							   inDirectory:dir];
   NSURL * 			url 	= [NSURL fileURLWithPath:name];
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
							style, FALSE, fAnisotropy)];	
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
		    load_texture(@"selected_piece_texture", nil, FALSE, fAnisotropy)];
	fSelectedPieceDrawStyle->fAlpha	= 0.8f;

    for (char i = '1'; i <= '8'; ++i) 
        fNumberTextures[i - '1'] = 
			load_texture([NSString stringWithFormat:@"%c", i], nil, TRUE, fAnisotropy);

    for (char i = 'a'; i <= 'h'; i++) 
        fLetterTextures[i - 'a'] = 
			load_texture([NSString stringWithFormat:@"%c", i], nil, TRUE, fAnisotropy);
}

- (void) loadColors
{
	[self loadStaticTextures];
}

@end

// Local Variables:
// mode:ObjC
// End:
