/*
	File:		MBCBoardViewTextures.mm
	Contains:	Load OpenGL textures from resources
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.
	
	Derived from glChess, Copyright © 2002 Robert Ancell and Michael Duelli
	Permission granted to Apple to relicense under the following terms:

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardViewTextures.mm,v $
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

#import <stdio.h>
#import <stdlib.h> 
#import <string.h>
#import <OpenGL/glu.h>
#import <OpenGL/glext.h>
#import <GLUT/glut.h>

void
bwtorgba(unsigned char *b,unsigned char *l,int n) {
    while(n--) {
        l[0] = *b;
        l[1] = *b;
        l[2] = *b;
        l[3] = 0xff;
        l += 4; b++;
    }
}

void
rgbtorgba(unsigned char *r,unsigned char *g,unsigned char *b,unsigned char *l,int n) {
    while(n--) {
        l[0] = r[0];
        l[1] = g[0];
        l[2] = b[0];
        l[3] = 0xff;
        l += 4; r++; g++; b++;
    }
}

void
rgbatorgba(unsigned char *r,unsigned char *g,unsigned char *b,unsigned char *a,unsigned char *l,int n) {
    while(n--) {
        l[0] = r[0];
        l[1] = g[0];
        l[2] = b[0];
        l[3] = a[0];
        l += 4; r++; g++; b++; a++;
    }
}

typedef struct _ImageRec {
    unsigned short imagic;
    unsigned short type;
    unsigned short dim;
    unsigned short xsize, ysize, zsize;
    unsigned int min, max;
    unsigned int wasteBytes;
    char name[80];
    unsigned long colorMap;
    FILE *file;
    unsigned char *tmp, *tmpR, *tmpG, *tmpB;
    unsigned long rleEnd;
    unsigned int *rowStart;
    int *rowSize;
} ImageRec;

static void
ConvertShort(unsigned short *array, unsigned int length) {
    unsigned short b1, b2;
    unsigned char *ptr;

    ptr = (unsigned char *)array;
    while (length--) {
        b1 = *ptr++;
        b2 = *ptr++;
        *array++ = (b1 << 8) | (b2);
    }
}

static void
ConvertUint(unsigned *array, unsigned int length) {
    unsigned int b1, b2, b3, b4;
    unsigned char *ptr;

    ptr = (unsigned char *)array;
    while (length--) {
        b1 = *ptr++;
        b2 = *ptr++;
        b3 = *ptr++;
        b4 = *ptr++;
        *array++ = (b1 << 24) | (b2 << 16) | (b3 << 8) | (b4);
    }
}

static ImageRec *ImageOpen(const char *fileName)
{
    union {
        int testWord;
        char testByte[4];
    } endianTest;
    ImageRec *image;
    int swapFlag;
    int x;

    endianTest.testWord = 1;
    if (endianTest.testByte[0] == 1) {
        swapFlag = 1;
    } else {
        swapFlag = 0;
    }

    image = (ImageRec *)malloc(sizeof(ImageRec));
    if (image == NULL) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    if ((image->file = fopen(fileName, "rb")) == NULL) {
        perror(fileName);
        exit(1);
    }

    fread(image, 1, 12, image->file);

    if (swapFlag) {
        ConvertShort(&image->imagic, 6);
    }

    image->tmp = (unsigned char *)malloc(image->xsize*256);
    image->tmpR = (unsigned char *)malloc(image->xsize*256);
    image->tmpG = (unsigned char *)malloc(image->xsize*256);
    image->tmpB = (unsigned char *)malloc(image->xsize*256);
    if (image->tmp == NULL || image->tmpR == NULL || image->tmpG == NULL ||
        image->tmpB == NULL) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }

    if ((image->type & 0xFF00) == 0x0100) {
        x = image->ysize * image->zsize * (int) sizeof(unsigned);
        image->rowStart = (unsigned *)malloc(x);
        image->rowSize = (int *)malloc(x);
        if (image->rowStart == NULL || image->rowSize == NULL) {
            fprintf(stderr, "Out of memory!\n");
            exit(1);
        }
        image->rleEnd = 512 + (2 * x);
        fseek(image->file, 512, SEEK_SET);
        fread(image->rowStart, 1, x, image->file);
        fread(image->rowSize, 1, x, image->file);
        if (swapFlag) {
            ConvertUint(image->rowStart, x/(int) sizeof(unsigned));
            ConvertUint((unsigned *)image->rowSize, x/(int) sizeof(int));
        }
    } else {	
		image->rowStart	= NULL;
		image->rowSize  = NULL;
	}

    return image;
}

static void
ImageClose(ImageRec *image) {
    fclose(image->file);
    free(image->tmp);
    free(image->tmpR);
    free(image->tmpG);
    free(image->tmpB);
	if (image->rowStart) {
		//		free(image->rowStart);
		// 		free(image->rowSize);
	}
    free(image);
}

static void
ImageGetRow(ImageRec *image, unsigned char *buf, int y, int z) {
    unsigned char *iPtr, *oPtr, pixel;
    int count;

    if ((image->type & 0xFF00) == 0x0100) {
        fseek(image->file, (long) image->rowStart[y+z*image->ysize], SEEK_SET);
        fread(image->tmp, 1, (unsigned int)image->rowSize[y+z*image->ysize],
              image->file);

        iPtr = image->tmp;
        oPtr = buf;
        for (;;) {
            pixel = *iPtr++;
            count = (int)(pixel & 0x7F);
            if (!count) {
				if (oPtr-buf != image->xsize)
					printf("Oops! %d != %d\n", oPtr-buf, image->xsize);
                return;
            }
            if (pixel & 0x80) {
                while (count--) {
                    *oPtr++ = *iPtr++;
                }
            } else {
                pixel = *iPtr++;
                while (count--) {
                    *oPtr++ = pixel;
                }
            }
        }
    } else {
        fseek(image->file, 512+(y*image->xsize)+(z*image->xsize*image->ysize),
              SEEK_SET);
        fread(buf, 1, image->xsize, image->file);
    }
}

unsigned *
read_texture(const char *name, int *width, int *height, int *components) {
    unsigned *base, *lptr;
    unsigned char *rbuf, *gbuf, *bbuf, *abuf;
    ImageRec *image;
    int y;

    image = ImageOpen(name);
    
    if(!image)
        return NULL;
    (*width)=image->xsize;
    (*height)=image->ysize;
    (*components)=image->zsize;
    base = (unsigned *)malloc(image->xsize*image->ysize*sizeof(unsigned));
    rbuf = (unsigned char *)malloc(image->xsize*sizeof(unsigned char));
    gbuf = (unsigned char *)malloc(image->xsize*sizeof(unsigned char));
    bbuf = (unsigned char *)malloc(image->xsize*sizeof(unsigned char));
    abuf = (unsigned char *)malloc(image->xsize*sizeof(unsigned char));
    if(!base || !rbuf || !gbuf || !bbuf)
      return NULL;
    lptr = base;
    for(y=0; y<image->ysize; y++) {
        if(image->zsize>=4) {
            ImageGetRow(image,rbuf,y,0);
            ImageGetRow(image,gbuf,y,1);
            ImageGetRow(image,bbuf,y,2);
            ImageGetRow(image,abuf,y,3);
            rgbatorgba(rbuf,gbuf,bbuf,abuf,(unsigned char *)lptr,image->xsize);
            lptr += image->xsize;
        } else if(image->zsize==3) {
            ImageGetRow(image,rbuf,y,0);
            ImageGetRow(image,gbuf,y,1);
            ImageGetRow(image,bbuf,y,2);
            rgbtorgba(rbuf,gbuf,bbuf,(unsigned char *)lptr,image->xsize);
            lptr += image->xsize;
        } else {
            ImageGetRow(image,rbuf,y,0);
            bwtorgba(rbuf,(unsigned char *)lptr,image->xsize);
            lptr += image->xsize;
        }
    }
    ImageClose(image);
    free(rbuf);
    free(gbuf);
    free(bbuf);
    free(abuf);

    return (unsigned *) base;
}

GLuint load_texture(NSString * name, NSString * dir, BOOL mono)
{
    GLuint	texture_name;
    GLubyte *	data;
    int 	w, h, c;
    name = [[NSBundle mainBundle] pathForResource:name 
							   ofType:@"rgb" 
							   inDirectory:dir];
    data = (GLubyte *)read_texture([name cString], &w, &h, &c);
    
    if (mono) 
		for (c = 0; c<w*h; ++c) {
			float v = data[4*c]/255.0f;
			data[4*c+0]	= 255;
			data[4*c+1] = 255;
			data[4*c+2] = 255;
			data[4*c+3] = static_cast<GLubyte>(pow(v,0.8)*255.0f);
		}
     
    /* Generate the texture */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture_name);
    glBindTexture(GL_TEXTURE_2D, texture_name);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4);

    gluBuild2DMipmaps(GL_TEXTURE_2D, 4, w, h, GL_RGBA,  GL_UNSIGNED_BYTE, data);
            
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
							style, FALSE)];	
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
		    load_texture(@"selected_piece_texture", nil, FALSE)];
	fSelectedPieceDrawStyle->fAlpha	= 0.8f;

    for (char i = '1'; i <= '8'; ++i) 
        fNumberTextures[i - '1'] = 
			load_texture([NSString stringWithFormat:@"%c", i], nil, TRUE);

    for (char i = 'a'; i <= 'h'; i++) 
        fLetterTextures[i - 'a'] = 
			load_texture([NSString stringWithFormat:@"%c", i], nil, TRUE);
}

- (void) loadColors
{
	[self loadStaticTextures];
}

@end

// Local Variables:
// mode:ObjC
// End:
