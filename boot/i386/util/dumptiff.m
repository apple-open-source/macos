/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Dump a tiff file into a format that is easily
 * used by the booter.
 *
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#import <stdio.h>
#import <sys/param.h>
#import "bitmap.h"
#import "cursor.h"
#import "BooterBitmap.h"

#define DEFAULT_CURSOR_NAME "ns_wait"

void printCursors(char *name, int bg_color, int use_c_mode)
{
    id bitmap;
    char buf[MAXPATHLEN];
    bitmap = [[BooterBitmap alloc] init];
    
    [bitmap setWidth:16];
    [bitmap setHeight:16];
    [bitmap setColorDataBytes:64];
    [bitmap setBgColor:bg_color];
    [bitmap setTwoBitsPerPixelAlphaData:(unsigned char *)waitAlpha2];

    sprintf(buf,"%s1",name);
    [bitmap setTwoBitsPerPixelColorData:waitData2W1];
    use_c_mode ? [bitmap writeAsCFile:buf] :
	[bitmap writeAsBinaryFile:buf];
    
    sprintf(buf,"%s2",name);
    [bitmap setTwoBitsPerPixelColorData:waitData2W2];
    use_c_mode ? [bitmap writeAsCFile:buf] :
	[bitmap writeAsBinaryFile:buf];
    
    sprintf(buf,"%s3",name);
    [bitmap setTwoBitsPerPixelColorData:waitData2W3];
    use_c_mode ? [bitmap writeAsCFile:buf] :
	[bitmap writeAsBinaryFile:buf];
    
    [bitmap free];
}

void usage(void)
{
    fprintf(stderr,"Usage: dumptiff [-b <bgcolor] [-c] [-C] [-o <ofile>] <tiff>\n");
    fprintf(stderr,"-C prints cursor bitmaps\n");
    fprintf(stderr,"-c creates files <tiff>.h and <tiff>_bitmap.h\n");
    fprintf(stderr,"(default is to create binary .bitmap file)\n");
    exit(1);
}

void
main(int argc, char **argv)
{
    id bitmap;
    char buf[MAXPATHLEN], *file;
    int vflag=0, errflag=0, pcursors=0, c, ret;
    extern char *optarg;
    extern int optind;
    int bg_color = BG_COLOR;
    int use_c_mode = 0;
    char *output_name = NULL;
    
    while ((c = getopt(argc, argv, "Ccvb:o:")) != EOF)
	switch (c) {
	case 'C':
	    pcursors++;
	    break;
	case 'c':
	    use_c_mode++;
	    break;
	case 'v':
	    vflag++;
	    break;
	case 'b':
	    bg_color = atoi(optarg);
	    break;
	case 'o':
	    output_name = optarg;
	    break;
	default:
	    errflag++;
	    break;
	}

    if (pcursors && !errflag) {
	if (output_name == NULL)
	    output_name = DEFAULT_CURSOR_NAME;
	printCursors(output_name, bg_color, use_c_mode);
	exit(0);
    }
    
    if (errflag || (optind != argc-1))
	usage();
    
    file = argv[optind];
    if (strcmp(file + strlen(file) - strlen(".tiff"), ".tiff") != 0)
	sprintf(buf,"%s.tiff",file);
    else
	sprintf(buf,"%s",file);
    bitmap = [[BooterBitmap alloc] initFromTiffFile:buf];
    if (bitmap == nil) {
	fprintf(stderr, "Could not create booter bitmap object\n");
	exit(1);
    }
    [bitmap setBgColor:bg_color];

    buf[strlen(buf) - strlen(".tiff")] = '\0';
    if (output_name == NULL)
	output_name = buf;

    ret = use_c_mode ?
	[bitmap writeAsCFile: output_name] :
	[bitmap writeAsBinaryFile: output_name];
    
    [bitmap free];
    exit(ret);
}
    