/*
cc -g -o /tmp/img img.c -framework ApplicationServices -framework IOKit -Wall
*/

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>
#include <stdio.h>

static void spit(char * filename)
{
    CFStringRef string;
    CFURLRef   url;
    char                        path[256];

    snprintf(path, sizeof(path), "%s.png", filename);

    string = CFStringCreateWithCString( kCFAllocatorDefault, path,
                                        kCFStringEncodingMacRoman );

    url = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, string, kCFURLPOSIXPathStyle, FALSE);

    CGImageSourceRef isrc = CGImageSourceCreateWithURL(url, nil);
    CGImageRef        imageRef;

    imageRef = CGImageSourceCreateImageAtIndex(isrc, 0, NULL);
    CFRelease(isrc);

    size_t pixelsWide = CGImageGetWidth(imageRef);
    size_t pixelsHigh = CGImageGetHeight(imageRef);
//    printf("%ld x %ld @ %ld\n", pixelsWide, pixelsHigh, CGImageGetBitsPerPixel(imageRef));

    CGContextRef    context = NULL;
    CGColorSpaceRef colorSpace;
    void *          bitmapData;
    int             bitmapByteCount;
    int             bitmapBytesPerRow;
 
    bitmapBytesPerRow   = (pixelsWide * 1);// 1
    bitmapByteCount     = (bitmapBytesPerRow * pixelsHigh);
 
//    colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);// 2
    colorSpace = CGColorSpaceCreateDeviceGray();// 2
    bitmapData = calloc( bitmapByteCount, sizeof(char) );// 3
    if (bitmapData == NULL)
    {
        fprintf (stderr, "Memory not allocated!");
    }
    context = CGBitmapContextCreate (bitmapData,// 4
                                    pixelsWide,
                                    pixelsHigh,
                                    8,      // bits per component
                                    bitmapBytesPerRow,
                                    colorSpace,
//                                    kCGImageAlphaPremultipliedLast);
                                    kCGImageAlphaNone);
    if (context== NULL)
    {
        free (bitmapData);// 5
        fprintf (stderr, "Context not created!");
    }
    CGColorSpaceRelease( colorSpace ); 

//    printf("%p\n", context);

    CGRect bounds = CGRectMake (0, 0, pixelsWide, pixelsHigh);    

    CGContextSetGrayFillColor(context, 0xbf / 255.0, 1.00);
    CGContextFillRect(context, bounds);

    CGContextDrawImage(context, bounds, imageRef);

    size_t x, y;
    uint8_t * bits;
    unsigned char data;

#if 0
    bits = (uint8_t *) bitmapData;
    for (y = 0; y < pixelsHigh; y++)
    {
        for (x = 0; x < pixelsWide; x++)
        {
            data = *bits++;
            assert (data <= 0xbf);

            printf("0x%02x, ", data);
        }
        printf("\n");
    }
#endif
    printf("\n\nunsigned char %s[%d * %d] = {\n", filename, pixelsWide, pixelsHigh);

    bits = (uint8_t *) bitmapData;
    for (y = 0; y < pixelsHigh; y++)
    {
        for (x = 0; x < pixelsWide; x++)
        {
            unsigned char data;
            data = *bits++;
            assert (data <= 0xbf);

            data = (0x100 * data - (0xbf / 2)) / 0xbf;

            if (!x) printf("    ");
            printf("0x%02x, ", data);
        }
        printf("\n");
    }
    printf("};\n");

}

int main(int argc, char * argv[])
{
    int i;
    for (i = 1; i < argc; i++)
        spit(argv[i]);
}



