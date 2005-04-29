/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 *  BLGenerateOFLabel.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Sat Feb 23 2002.
 *  Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGenerateOFLabel.c,v 1.20 2005/02/08 05:17:22 ssen Exp $
 *
 *  $Log: BLGenerateOFLabel.c,v $
 *  Revision 1.20  2005/02/08 05:17:22  ssen
 *  fix some open source usage
 *
 *  Revision 1.19  2005/02/03 00:42:27  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.18  2004/04/20 21:40:44  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.17  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.16  2003/08/04 05:24:16  ssen
 *  Add #ifndef _OPEN_SOURCE so that some stuff isn't in darwin
 *
 *  Revision 1.15  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.14  2003/04/19 00:11:12  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.13  2003/04/17 00:59:36  ssen
 *  truncate to 341 pixels if too wide
 *
 *  Revision 1.12  2003/04/16 23:57:33  ssen
 *  Update Copyrights
 *
 *  Revision 1.11  2003/03/26 00:33:31  ssen
 *  Use _OPEN_SOURCE_ instead of DARWIN, by Rob's request
 *
 *  Revision 1.10  2003/03/20 03:41:03  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.9.2.1  2003/03/20 03:32:36  ssen
 *  swap height and width of OF label header
 *
 *  Revision 1.9  2003/03/19 22:57:06  ssen
 *  C99 types
 *
 *  Revision 1.7  2003/03/18 23:51:31  ssen
 *  Use CG directory
 *
 *  Revision 1.6  2002/06/11 00:50:49  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.5  2002/05/03 04:23:55  ssen
 *  Consolidate APIs, and update bless to use it
 *
 *  Revision 1.4  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.3  2002/03/05 01:47:53  ssen
 *  Add CG compat files and dynamic loading
 *
 *  Revision 1.2  2002/03/04 22:25:05  ssen
 *  implement CLUT for antialiasing
 *
 *  Revision 1.1  2002/02/24 11:30:52  ssen
 *  Add OF label support
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>

#include "bless.h"
#include "bless_private.h"

static const unsigned char clut[] =
  {
    0x00, /* 0x00 0x00 0x00 white */
    0xF6, /* 0x11 0x11 0x11 */
    0xF7, /* 0x22 0x22 0x22 */

    0x2A, /* 0x33 = 1*6^2 + 1*6 + 1 = 43 colors */

    0xF8, /* 0x44 */
    0xF9, /* 0x55 */

    0x55, /* 0x66 = 2*(36 + 6 + 1) = 86 colors */

    0xFA, /* 0x77 */
    0xFB, /* 0x88 */

    0x80, /* 0x99 = (3*43) = 129 colors*/

    0xFC, /* 0xAA */
    0xFD, /* 0xBB */

    0xAB, /* 0xCC = 4*43 = 172 colors */

    0xFE, /* 0xDD */
    0xFF, /* 0xEE */

    0xD6, /* 0xFF = 5*43 = 215 */
  };

static int makeLabelOfSize(const char *label, char *bitmapData,
        uint16_t width, uint16_t height, uint16_t *newwidth);

static int refitToWidth(char *bitmapData,
        uint16_t width, uint16_t height, uint16_t newwidth);

int BLGenerateOFLabel(BLContextPtr context,
                    const unsigned char label[],
                    CFDataRef* data) {
                    
                    
        uint16_t width = 340;
        uint16_t height = 12;
        uint16_t newwidth;
        int err;
        int i;
        CFDataRef bits = NULL;
        unsigned char *bitmapData;

        contextprintf(context, kBLLogLevelError,
		      "CoreGraphics is not available for rendering\n");
	return 1;
	
        bitmapData = malloc(width*height+5);
        if(!bitmapData) {
                contextprintf(context, kBLLogLevelError,
                    "Could not alloc CoreGraphics backing store\n");
                return 1;
        }
        bzero(bitmapData, width*height+5);

        err = makeLabelOfSize(label, bitmapData+5, width, height, &newwidth);
	if(err) {
	  free(bitmapData);
	  *data = NULL;
	  return 2;
	}

	// cap at 300 pixels wide.
	if(newwidth > width) newwidth = width;
	
	err = refitToWidth(bitmapData+5, width, height, newwidth);
	if(err) {
	  free(bitmapData);
	  *data = NULL;
	  return 3;
	}

	bitmapData = realloc(bitmapData, newwidth*height+5);
	if(NULL == bitmapData) {
                contextprintf(context, kBLLogLevelError,
                    "Could not realloc to shrink CoreGraphics backing store\n");
		
                return 4;
	}

        bitmapData[0] = 1;
        *(uint16_t *)&bitmapData[1] = CFSwapInt16HostToBig(newwidth);
        *(uint16_t *)&bitmapData[3] = CFSwapInt16HostToBig(height);
        
        for(i=5; i < newwidth*height+5; i++) {
            bitmapData[i] = clut[bitmapData[i] >> 4];
        }

	//	bits = CFDataCreate(kCFAllocatorDefault, bitmapData, newwidth*height+5);
	bits = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bitmapData, newwidth*height+5, kCFAllocatorMalloc);
	//        free(bitmapData);
        
	if(bits == NULL) {
                contextprintf(context, kBLLogLevelError,
                    "Could not create CFDataRef\n");
		return 6;
	}

        *data = (void *)bits;
        
        return 0;
}

static int makeLabelOfSize(const char *label, char *bitmapData,
						   uint16_t width, uint16_t height, uint16_t *newwidth) {
	return 1;
}

/*
 * data is of the form:
 *  111111000111111000111111000 ->
 *  111111111111111111
 */

static int refitToWidth(char *bitmapData,
        uint16_t width, uint16_t height, uint16_t newwidth)
{
  uint16_t row;
  for(row=0; row < height; row++) {
    bcopy(&bitmapData[row*width], &bitmapData[row*newwidth], newwidth);
  }

  return 0;
}
