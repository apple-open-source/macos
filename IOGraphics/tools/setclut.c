/*cc -o /tmp/setclut setclut.c -framework IOKit -framework ApplicationServices -Wall -g
*/

#include <ApplicationServices/ApplicationServicesPriv.h>

int main( int argc, char * argv[] )
{
    CGSPaletteRef palette;
    CGSDeviceColor color;

    palette = CGSPaletteCreateDefaultColorPalette();

    while(1)
    {
        color.red = 1.0;
        color.green = 0.0;
        color.blue = 0.0;
        CGSPaletteSetColorAtIndex(palette, color, 0);
        CGSSetDisplayPalette(CGSMainDisplayID(), palette);
    
        color.red = 0.0;
        color.green = 1.0;
        color.blue = 0.0;
        CGSPaletteSetColorAtIndex(palette, color, 0);
        CGSSetDisplayPalette(CGSMainDisplayID(), palette);
    }
    exit(0);
}


