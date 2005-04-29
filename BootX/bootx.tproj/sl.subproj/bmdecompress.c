/*
 * Copyright (c) 1995-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sl.h>
#include <stdint.h>

typedef uint8_t  UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef int8_t   SInt8;
typedef int16_t  SInt16;
typedef int32_t  SInt32;

enum { false = 0, true = 1 };

static void 
PreviewDecompress16(uint32_t * compressBuffer, 
                        uint32_t width, uint32_t height, uint32_t row, 
                        uint16_t * output)
{
    int i, j;
    uint32_t * input;
    
    uint16_t * sc0 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc1 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc2 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc3 = malloc((width+2) * sizeof(uint16_t));
    uint32_t   sr0, sr1, sr2, sr3;

    bzero(sc0, (width+2) * sizeof(uint16_t));
    bzero(sc1, (width+2) * sizeof(uint16_t));
    bzero(sc2, (width+2) * sizeof(uint16_t));
    bzero(sc3, (width+2) * sizeof(uint16_t));

    uint32_t tmp1, tmp2, out;
    for (j = 0; j < (height + 2); j++)
    {
        input = compressBuffer;
        if (j < height)
            input += j;
        else
            input += height - 1;
        input = (uint32_t *)(input[3] + ((uint8_t *)compressBuffer));

        uint32_t data, repeat, fetch, count = 0;
        sr0 = sr1 = sr2 = sr3 = 0;

        for (i = 0; i < (width + 2); i++)
        {
            if (i < width)
            {
                if (!count)
                {
                    count = *input++;
                    repeat = (count & 0xff000000);
                    count ^= repeat;
                    fetch = true;
                }
                else
                    fetch = (0 == repeat);
    
                count--;
    
                if (fetch)
                {
                    data = *((uint16_t *)input)++;
    
                    // grayscale
                    // srgb 13933, 46871, 4732
                    // ntsc 19595, 38470, 7471
                    data = 13933 * (0x1f & (data >> 10))
                         + 46871 * (0x1f & (data >> 5))
                         +  4732 * (0x1f & data);
                    data >>= 13;
        
                    // 70% white, 30 % black
                    data *= 19661;
                    data += (103 << 16);
                    data >>= 16;
                }
            }

            // gauss blur
            tmp2 = sr0 + data;
            sr0 = data;
            tmp1 = sr1 + tmp2;
            sr1 = tmp2;
            tmp2 = sr2 + tmp1;
            sr2 = tmp1;
            tmp1 = sr3 + tmp2;
            sr3 = tmp2;
            
            tmp2 = sc0[i] + tmp1;
            sc0[i] = tmp1;
            tmp1 = sc1[i] + tmp2;
            sc1[i] = tmp2;
            tmp2 = sc2[i] + tmp1;
            sc2[i] = tmp1;
            out = (128 + sc3[i] + tmp2) >> 11;
            sc3[i] = tmp2;

            out &= 0x1f;
            if ((i > 1) && (j > 1))
                output[i-2] = out | (out << 5) | (out << 10);
        }

        if (j > 1)
            output += row;
    }
    free(sc3);
    free(sc2);
    free(sc1);
    free(sc0);
}

static void 
PreviewDecompress32(uint32_t * compressBuffer, 
                        uint32_t width, uint32_t height, uint32_t row, 
                        uint32_t * output)
{
    int i, j;
    uint32_t * input;
    
    uint16_t * sc0 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc1 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc2 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc3 = malloc((width+2) * sizeof(uint16_t));
    uint32_t   sr0, sr1, sr2, sr3;

    bzero(sc0, (width+2) * sizeof(uint16_t));
    bzero(sc1, (width+2) * sizeof(uint16_t));
    bzero(sc2, (width+2) * sizeof(uint16_t));
    bzero(sc3, (width+2) * sizeof(uint16_t));

    uint32_t tmp1, tmp2, out;
    for (j = 0; j < (height + 2); j++)
    {
        input = compressBuffer;
        if (j < height)
            input += j;
        else
            input += height - 1;
        input = (uint32_t *)(input[3] + ((uint8_t *)compressBuffer));

        uint32_t data, repeat, fetch, count = 0;
        sr0 = sr1 = sr2 = sr3 = 0;

        for (i = 0; i < (width + 2); i++)
        {
            if (i < width)
            {
                if (!count)
                {
                    count = *input++;
                    repeat = (count & 0xff000000);
                    count ^= repeat;
                    fetch = true;
                }
                else
                    fetch = (0 == repeat);
    
                count--;
    
                if (fetch)
                {
                    data = *input++;
    
                    // grayscale
                    // srgb 13933, 46871, 4732
                    // ntsc 19595, 38470, 7471
                    data = 13933 * (0xff & (data >> 24))
                         + 46871 * (0xff & (data >> 16))
                         +  4732 * (0xff & data);
                    data >>= 16;
        
                    // 70% white, 30 % black
                    data *= 19661;
                    data += (103 << 16);
                    data >>= 16;
                }
            }

            // gauss blur
            tmp2 = sr0 + data;
            sr0 = data;
            tmp1 = sr1 + tmp2;
            sr1 = tmp2;
            tmp2 = sr2 + tmp1;
            sr2 = tmp1;
            tmp1 = sr3 + tmp2;
            sr3 = tmp2;
            
            tmp2 = sc0[i] + tmp1;
            sc0[i] = tmp1;
            tmp1 = sc1[i] + tmp2;
            sc1[i] = tmp2;
            tmp2 = sc2[i] + tmp1;
            sc2[i] = tmp1;
            out = (128 + sc3[i] + tmp2) >> 8;
            sc3[i] = tmp2;

            out &= 0xff;
            if ((i > 1) && (j > 1))
                output[i-2] = out | (out << 8) | (out << 16);
        }

        if (j > 1)
            output += row;
    }

    free(sc3);
    free(sc2);
    free(sc1);
    free(sc0);
}

int
DecompressData(void *srcbase, void *dstbase,
		    int dw, int dh, int bytesPerPixel, int rowbytes)
{
    uint32_t * src = (uint32_t *) srcbase;

    if ((bytesPerPixel != (int) src[0]) || (dw != (int) src[1]) || (dh != (int) src[2]))
      return (false);

    switch(bytesPerPixel)
    {
      case 4:
        PreviewDecompress32((uint32_t *)srcbase, dw, dh, rowbytes >> 2, (uint32_t *) dstbase);
        return (true);
      case 2:
        PreviewDecompress16((uint32_t *)srcbase, dw, dh, rowbytes >> 1, (uint16_t *) dstbase);
        return (true);
      default:
        return (false);
    }
}
