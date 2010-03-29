/*cc -o /tmp/capture capture.c -framework IOKit -framework ApplicationServices -Wall -g
*/


#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/i2c/IOI2CInterface.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_DISPLAYS    16

int main( int argc, char * argv[] )
{
    CGDirectDisplayID dispids[MAX_DISPLAYS];
    CGDisplayCount    ndid, idx;
    CGError err;
    char c;

    CGGetActiveDisplayList(MAX_DISPLAYS, dispids, &ndid);

    for (idx = 0; idx < ndid; idx++)
    {
        err = CGDisplayCaptureWithOptions(dispids[idx], kCGCaptureNoFill);
        printf("CGDisplayCapture(%x) %d\n", dispids[idx], err);
        CGDisplayHideCursor(dispids[idx]);
    }

    c = getchar();

    for (idx = 0; idx < ndid; idx++)
    {
        err = CGDisplayRelease(dispids[idx]);
        printf("CGDisplayRelease(%x) %d\n", dispids[idx], err);
        CGDisplayShowCursor(dispids[idx]);
    }

    return (0);
}
