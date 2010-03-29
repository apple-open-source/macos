/*
cc -Os -o /tmp/gaussblur gaussblur.c -framework ApplicationServices -framework IOKit -Wall
*/

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach/mach.h>
#include <mach/mach_time.h>


#define DEBG(idx, fmt, args...)         \
do {                                    \
    printf(fmt, ## args);               \
} while( false )
#define bcopy_nc bcopy

#include "../IOGraphicsFamily/bmcompress.h"


int main(int argc, char * argv[])
{
    CGError             err;
    int                 i, j;
    CGDisplayCount      max;
    CGDirectDisplayID   displayIDs[8];
    double scale = 0.0;
    uint64_t start, end;

    uint32_t * screen;
    uint32_t rowBytes, bytesPerPixel;
    uint32_t width, height;
    uint32_t * buffer;
    uint32_t * compressBuffer;
    uint32_t * input;
    uint32_t * output;
    uint32_t   compressLen;
    
    {
        struct mach_timebase_info tbi;
        kern_return_t r;
        uint32_t num = 0;
        uint32_t denom = 0;

        r = mach_timebase_info(&tbi);
        if (r != KERN_SUCCESS) {
            abort();
        }
        num = tbi.numer;
        denom = tbi.denom;

        scale = (double)num / (double)denom;
    }

    err = CGGetOnlineDisplayList(8, displayIDs, &max);
    if(err != kCGErrorSuccess)
        exit(1);
    if(max > 8)
        max = 8;

    screen = (uint32_t *) CGDisplayBaseAddress(displayIDs[0]);
    rowBytes = CGDisplayBytesPerRow(displayIDs[0]);

    printf("Base addr %p, rb %x\n", screen, rowBytes);

    rowBytes >>= 2;

    width = CGDisplayPixelsWide(displayIDs[0]);
    height = CGDisplayPixelsHigh(displayIDs[0]);

    buffer = malloc(width * height * sizeof(uint32_t));
    start = mach_absolute_time();
    for (j = 0; j < height; j++)
        bcopy(screen + j * rowBytes, buffer + j * width, width * sizeof(uint32_t));
    end = mach_absolute_time();

    printf("copy time %f\n", (end - start) * scale / NSEC_PER_SEC);
    printf("copy Mbs %f\n", ((double) width * height * sizeof(uint32_t)) / 1024.0 / 1024.0 / ((end - start) * scale / NSEC_PER_SEC) );


bytesPerPixel = sizeof(uint32_t);

    compressBuffer = malloc(width * height * sizeof(uint32_t) * 2);

//screen = buffer;
//rowBytes = width;


    start = mach_absolute_time();
    compressLen = CompressData( (UInt8 *) screen, bytesPerPixel,
                            width, height, rowBytes << 2,
                            (UInt8 *) compressBuffer, width * height * sizeof(uint32_t) * 2 );
    end = mach_absolute_time();

    printf("compress time %f\n", (end - start) * scale / NSEC_PER_SEC);

    DEBG(thisIndex, " compressed to %d%%\n", (compressLen * 100) / (width * height * sizeof(uint32_t)));

//exit(0);

    uint16_t * sc0 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc1 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc2 = malloc((width+2) * sizeof(uint16_t));
    uint16_t * sc3 = malloc((width+2) * sizeof(uint16_t));
    uint32_t   sr0, sr1, sr2, sr3;

    bzero(sc0, (width+2) * sizeof(uint16_t));
    bzero(sc1, (width+2) * sizeof(uint16_t));
    bzero(sc2, (width+2) * sizeof(uint16_t));
    bzero(sc3, (width+2) * sizeof(uint16_t));

    input = buffer;
    output = screen;

    start = mach_absolute_time();
    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
            output[i] = *input++;
        output += rowBytes;
    }

    end = mach_absolute_time();

    printf("time %f\n", (end - start) * scale / NSEC_PER_SEC);

    output = screen;

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
            output += rowBytes;
    }

    end = mach_absolute_time();

    printf("time %f\n", (end - start) * scale / NSEC_PER_SEC);

    exit(0);
    return(0);
}

