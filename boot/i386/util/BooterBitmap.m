/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <stdio.h>
#import <sys/param.h>
#import <appkit/NXBitmapImageRep.h>
#import "BooterBitmap.h"
#import "bitmap.h"

@implementation BooterBitmap

- init
{
    bg_color = BG_COLOR;
    return [super init];
}

- free
{
    if (bitmapImageRep) [bitmapImageRep free];
    if (packed_planes[0]) free(packed_planes[0]);
    if (packed_planes[1]) free(packed_planes[1]);
    return [super free];
}

- initFromTiffFile: (char *)inputFileName;
{    
    [self init];
    filename = inputFileName;
    bitmapImageRep = [[NXBitmapImageRep alloc]
				initFromFile:inputFileName];
    if (bitmapImageRep == nil) {
    	fprintf(stderr, "BooterBitmap: couldn't load tiff file %s\n",filename);
	return nil;
    }
    if ([bitmapImageRep numPlanes] - [bitmapImageRep hasAlpha] != 1) {
	fprintf(stderr,
	    "BooterBitmap: can't deal with more than one input plane (excluding alpha)\n");
	return nil;
    }
    if ([bitmapImageRep bitsPerPixel] != BITS_PER_PIXEL) {
	fprintf(stderr,
	    "BooterBitmap: can't deal with anything but %d bits per pixel\n",
	    BITS_PER_PIXEL);
	return nil;
    }
    [bitmapImageRep getDataPlanes:planes];
    width = [bitmapImageRep pixelsWide];
    height = [bitmapImageRep pixelsHigh];
    bytes_per_plane = [bitmapImageRep bytesPerPlane];
    return self;
}

- (BOOL)_allocPlanes
{
    if (packed_planes[0]) free(packed_planes[0]);
    if (packed_planes[1]) free(packed_planes[1]);
    packed_planes[0] = (unsigned char *)malloc(bytes_per_plane / NPLANES);
    packed_planes[1] = (unsigned char *)malloc(bytes_per_plane / NPLANES);
    if (packed_planes[0] == 0 || packed_planes[1] == 0)
	return NO;
    else
	return YES;
}

- (BOOL)_convertPlanes
{
    int plane, i, j, outbit, index;
    unsigned char *pp, *alpha, *plane_tmp, *data;
    unsigned char alphabyte, inbyte, outbyte;
    int new_bytes_per_plane, bytes_per_row;
    TIFF tif;
    int doPack = 1;

startOver:
    if ([self _allocPlanes] == NO)
	return NO;
    plane_tmp = (unsigned char *)malloc(bytes_per_plane / NPLANES);
    for (plane = 0; plane < NPLANES; plane++) {
	int col;

	data = planes[0];
	alpha = planes[1];
	pp = plane_tmp;
	
	bytes_per_row = bytes_per_plane / height;

	for(i=0; i < height; i++) {
	    for(j = outbyte = col = 0, outbit = 7; j < width; j++) {
		if ((j % (8 / NPLANES)) == 0) {
		    index = (i * bytes_per_row) + (j / (8 / NPLANES));
		    inbyte = data[index];
		    if (alpha)
			alphabyte = alpha[index];
		}
		if (alpha && ((alphabyte & 0xC0) == 0)) {
		    outbyte |= 
			((bg_color & (1 << plane)) >> plane) << outbit;
		} else {
		    outbyte |= 
			((((inbyte & 0xC0) >> (8 - NPLANES)) & (1 << plane))
				>> plane) << outbit;
		}
		if (outbit-- == 0) {
		    *pp++ = outbyte;
		    outbyte = 0;
		    outbit = 7;
		}
		inbyte <<= NPLANES;
		alphabyte <<= NPLANES;
	    }
	    if (outbit < 7)
		*pp++ = outbyte;
	}
	bytes_per_row = (width + 7) / 8;
	new_bytes_per_plane = pp - plane_tmp;

	tif.tif_rawdata = tif.tif_rawcp = packed_planes[plane];
	tif.tif_rawdatasize = new_bytes_per_plane;
	tif.tif_rawcc = 0;
	tif.tif_row = 0;
	pp = plane_tmp;
	if (doPack) {
	    for (i=0; i < height; i++) {
		if (PackBitsEncode(&tif, pp, bytes_per_row, 0) == -1) {
		    // packed data is bigger than raw data!
		    doPack = 0;
		    free(plane_tmp);
		    goto startOver;
		}
		pp += bytes_per_row;
	    }
	} else {
	    bcopy(plane_tmp, packed_planes[plane], new_bytes_per_plane);
	    tif.tif_rawcc = new_bytes_per_plane;
	}
	plane_len[plane] = tif.tif_rawcc;
    }
    free(plane_tmp);
    packed = doPack;
    return YES;
}


- (BOOL) writeAsCFile: (char *)output_name
{
    char buf[MAXPATHLEN], oname_buf[MAXPATHLEN];
    char *name;
    FILE *bhfile, *hfile;
    int i, plane;
    
    if (output_name) {
	strcpy(oname_buf, output_name);
    } else if (filename) {
	strcpy(oname_buf, filename);
	buf[strlen(buf) - strlen(".tiff")] = '\0';
    } else {
	fprintf(stderr,"BooterBitmap writeAsCFile: no filename\n");
	return NO;
    }
    output_name = oname_buf;
    if ([self _convertPlanes] == NO) {
	fprintf(stderr,"_convertPlanes failed\n");
	return NO;
    }
    
    name = (char *)strrchr(output_name, '/');
    if (name == NULL)
	name = output_name;
    else
	name++;
    sprintf(buf, "%s_bitmap.h",output_name);
    bhfile = fopen(buf,"w");
    if (bhfile == 0) {
	fprintf(stderr,"Couldn't open %s for writing\n",buf);
	perror("open");
	return NO;
    }
    sprintf(buf,"%s.h",output_name);
    hfile = fopen(buf,"w");
    if (hfile == 0) {
	fprintf(stderr,"Couldn't open %s for writing\n",buf);
	perror("open");
	return NO;
    }
    
    for(plane = 0; plane < NPLANES; plane++) {
	int col = 0;
	unsigned char *pp;
	fprintf(bhfile,"unsigned char %s_bitmap_plane_%d[] =\n",name,plane);
	fprintf(bhfile," {\n");
    	fprintf(bhfile,"// plane %d\n",plane);
	pp = packed_planes[plane];
	for (i=0; i < plane_len[plane]; i++) {
	    fprintf(bhfile,"0x%02x, ",*pp++);
	    if ((col += 7) > 70) {
		col = 0;
		fprintf(bhfile,"\n");
	    }
	}
	fprintf(bhfile,"};\n");
    }
    
    fprintf(bhfile,"struct bitmap %s_bitmap = {\n",name);
    fprintf(bhfile,"%d,\t// packed\n",packed);
    fprintf(bhfile,"%d,\t// bytes_per_plane\n",bytes_per_plane / NPLANES);
    fprintf(bhfile,"%d,\t// bytes_per_row\n", (width + 7) / 8);
    fprintf(bhfile,"%d,\t// bits per pixel\n", 1);
    fprintf(bhfile,"%d,\t// width\n", width);
    fprintf(bhfile,"%d,\t// height\n", height);
    fprintf(bhfile,"{\n");
    fprintf(bhfile,"  %d,\n", plane_len[0]);
    fprintf(bhfile,"  %d,\n", plane_len[1]);
    fprintf(bhfile,"},\n");
    fprintf(bhfile,"{\n");
    fprintf(bhfile,"  %s_bitmap_plane_0,\n", name);
    fprintf(bhfile,"  %s_bitmap_plane_1\n", name);
    fprintf(bhfile,"}\n");
    fprintf(bhfile,"};\n");
    fprintf(bhfile,"\n#define %s_bitmap_WIDTH\t%d\n", name, width);
    fprintf(bhfile,"#define %s_bitmap_HEIGHT\t%d\n", name, height);
    fclose(bhfile);
    fprintf(hfile,"extern struct bitmap %s_bitmap;\n",name);
    fprintf(hfile,"\n#define %s_bitmap_WIDTH\t%d\n", name, width);
    fprintf(hfile,"#define %s_bitmap_HEIGHT\t%d\n", name, height);
    fclose(bhfile);
    return YES;
}

- (BOOL) writeAsBinaryFile: (char *)outputFile
{
    struct bitmap bd;
    char buf[MAXPATHLEN];
    FILE *file;
    
    if (outputFile) {
	strcpy(buf, outputFile);
    } else if (filename) {
	strcpy(buf, filename);
    } else {
	fprintf(stderr,"writeAsBinaryFile: no filename\n");
	return NO;
    }
    strcat(buf, ".image");
    file = fopen(buf, "w");
    if (file == NULL) {
	fprintf(stderr, "writeAsBinaryFile: couldn't open output file %s\n",
	    buf);
	return NO;
    }
    if ([self _convertPlanes] == NO) {
	fprintf(stderr,"_convertPlanes failed\n");
	return NO;
    }
    bd.packed = packed;
    bd.bytes_per_plane = bytes_per_plane / NPLANES;
    bd.bytes_per_row = (width + 7) / 8;
    bd.bits_per_pixel = 1;
    bd.width = width;
    bd.height = height;
    bd.plane_len[0] = plane_len[0];
    bd.plane_len[1] = plane_len[1];
    bd.plane_data[0] = bd.plane_data[1] = 0;
    if (fwrite(&bd, sizeof(bd), 1, file) < 1) goto error;
    if (fwrite(packed_planes[0], plane_len[0], 1, file) < 1) goto error;
    if (fwrite(packed_planes[1], plane_len[1], 1, file) < 1) goto error;
    fclose(file);
    return YES;
error:
    perror("fwrite");
    return NO;
}


- (int) width
{
    return width;
}

- (int) height
{
    return height;
}

- (int) setWidth: (int)newWidth
{
    return width = newWidth;
}

- (int) setHeight: (int)newHeight
{
    return height = newHeight;
}

- (int) bgColor
{
    return bg_color;
}

- (int) setBgColor: (int)newColor
{
    return bg_color = newColor;
}

- (BOOL) setTwoBitsPerPixelColorData: (unsigned char *)bits;
{
    planes[0] = bits;
    return YES;
}

- (BOOL) setTwoBitsPerPixelAlphaData: (unsigned char *)bits;
{
    planes[1] = bits;
    return YES;
}

- (unsigned char *) twoBitsPerPixelColorData
{
    return planes[0];
}

- (unsigned char *) twoBitsPerPixelAlphaData
{
    return planes[1];
}

- (int) colorDataBytes
{
    return bytes_per_plane;
}

- (int) setColorDataBytes: (int)bpp
{
    return bytes_per_plane = bpp;
}

- (char *)filename
{
    return filename;
}


@end
