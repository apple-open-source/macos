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
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#include "boot.h"
#include "vbe.h"
#include "appleClut8.h"
#include "appleboot.h"
#include "bootstruct.h"

/*
 * for spinning disk
 */
static int currentIndicator = 0;

static unsigned long lookUpCLUTIndex( unsigned char index,
                                      unsigned char depth );

static void drawColorRectangle( unsigned short x,
                                unsigned short y,
                                unsigned short width,
                                unsigned short height,
                                unsigned char  colorIndex );

static void drawDataRectangle( unsigned short  x,
                               unsigned short  y,
                               unsigned short  width,
                               unsigned short  height,
                               unsigned char * data );

#define VIDEO(x) (bootArgs->video.v_ ## x)


//==========================================================================
// printVBEInfo

void printVBEInfo()
{
    VBEInfoBlock vbeInfo;
    int          err;

    // Fetch VBE Controller Info.

    bzero( &vbeInfo, sizeof(vbeInfo) );
    err = getVBEInfo( &vbeInfo );
    if ( err != errSuccess )
        return;

    // Check presence of VESA signature.

    if ( strncmp( vbeInfo.VESASignature, "VESA", 4 ) )
        return;

    // Announce controller properties.

    printf("VESA v%d.%d %dMB (%s)\n",
           vbeInfo.VESAVersion >> 8,
           vbeInfo.VESAVersion & 0xf,
           vbeInfo.TotalMemory / 16,
           VBEDecodeFP(const char *, vbeInfo.OEMStringPtr) );
}

//==========================================================================
// getVESAModeWithProperties
//
// Return the VESA mode that matches the properties specified.
// If a mode is not found, then return the "best" available mode.

static unsigned short
getVESAModeWithProperties( unsigned short     width,
                           unsigned short     height,
                           unsigned char      bitsPerPixel,
                           unsigned short     attributesSet,
                           unsigned short     attributesClear,
                           VBEModeInfoBlock * outModeInfo,
                           unsigned short *   vesaVersion )
{
    VBEInfoBlock     vbeInfo;
    unsigned short * modePtr;
    VBEModeInfoBlock modeInfo;
    unsigned char    modeBitsPerPixel;
    unsigned short   matchedMode = modeEndOfList;
    int              err;

    // Clear output mode info.

    bzero( outModeInfo, sizeof(*outModeInfo) );

    // Get VBE controller info containing the list of supported modes.

    bzero( &vbeInfo, sizeof(vbeInfo) );
    err = getVBEInfo( &vbeInfo );
    if ( err != errSuccess )
    {
        return modeEndOfList;
    }

    // Report the VESA major/minor version number.

    if (vesaVersion) *vesaVersion = vbeInfo.VESAVersion;

    // Loop through the mode list, and find the matching mode.

    for ( modePtr = VBEDecodeFP( unsigned short *, vbeInfo.VideoModePtr );
          *modePtr != modeEndOfList; modePtr++ )
    {
        // Get mode information.

        bzero( &modeInfo, sizeof(modeInfo) );
        err = getVBEModeInfo( *modePtr, &modeInfo );
        if ( err != errSuccess )
        {
            continue;
        }

#if 0   // debug
        printf("Mode %x: %dx%dx%d mm:%d attr:%x\n",
               *modePtr, modeInfo.XResolution, modeInfo.YResolution,
               modeInfo.BitsPerPixel, modeInfo.MemoryModel,
               modeInfo.ModeAttributes);
#endif

        // Filter out unwanted modes based on mode attributes.

        if ( ( ( modeInfo.ModeAttributes & attributesSet ) != attributesSet )
        ||   ( ( modeInfo.ModeAttributes & attributesClear ) != 0 ) )
        {
            continue;
        }

        // Pixel depth in bits.

        modeBitsPerPixel = modeInfo.BitsPerPixel;

        if ( ( modeBitsPerPixel == 4 ) && ( modeInfo.MemoryModel == 0 ) )
        {
            // Text mode, 16 colors.
        }
        else if ( ( modeBitsPerPixel == 8 ) && ( modeInfo.MemoryModel == 4 ) )
        {
            // Packed pixel, 256 colors.
        }
        else if ( ( ( modeBitsPerPixel == 16 ) || ( modeBitsPerPixel == 15 ) )
        &&   ( modeInfo.MemoryModel   == 6 )
        &&   ( modeInfo.RedMaskSize   == 5 )
        &&   ( modeInfo.GreenMaskSize == 5 )
        &&   ( modeInfo.BlueMaskSize  == 5 ) )
        {
            // Direct color, 16 bpp (1:5:5:5).
            modeInfo.BitsPerPixel = modeBitsPerPixel = 16;
        }
        else if ( ( modeBitsPerPixel == 32 )
        &&   ( modeInfo.MemoryModel   == 6 )
        &&   ( modeInfo.RedMaskSize   == 8 )
        &&   ( modeInfo.GreenMaskSize == 8 )
        &&   ( modeInfo.BlueMaskSize  == 8 ) )
        {
            // Direct color, 32 bpp (8:8:8:8).
        }
        else
        {
            continue; // Not a supported mode.
        }

        // Modes larger than the specified dimensions are skipped.

        if ( ( modeInfo.XResolution > width  ) ||
             ( modeInfo.YResolution > height ) )
        {
            continue;
        }

        // Perfect match, we're done looking.

        if ( ( modeInfo.XResolution == width  ) &&
             ( modeInfo.YResolution == height ) &&
             ( modeBitsPerPixel     == bitsPerPixel ) )
        {
            matchedMode = *modePtr;
            bcopy( &modeInfo, outModeInfo, sizeof(modeInfo) );
            break;
        }

        // Save the next "best" mode in case a perfect match is not found.

        if ( modeInfo.XResolution == outModeInfo->XResolution &&
             modeInfo.YResolution == outModeInfo->YResolution &&
             modeBitsPerPixel     <= outModeInfo->BitsPerPixel )
        {
            continue;  // Saved mode has more depth.
        }
        if ( modeInfo.XResolution < outModeInfo->XResolution ||
             modeInfo.YResolution < outModeInfo->YResolution ||
             modeBitsPerPixel     < 16 )
        {
            continue;  // Saved mode has more resolution.
        }

        matchedMode = *modePtr;
        bcopy( &modeInfo, outModeInfo, sizeof(modeInfo) );
    }

    return matchedMode;
}

//==========================================================================
// setupPalette

static void setupPalette( VBEPalette * p, const unsigned char * g )
{
    int             i;
    unsigned char * source = (unsigned char *) g;

    for (i = 0; i < 256; i++)
    {
        (*p)[i] = 0;
        (*p)[i] |= ((unsigned long)((*source++) >> 2)) << 16;   // Red
        (*p)[i] |= ((unsigned long)((*source++) >> 2)) << 8;    // Green
        (*p)[i] |= ((unsigned long)((*source++) >> 2));         // Blue
    }
}

//==========================================================================
// Simple decompressor for boot images encoded in RLE format.

static char * decodeRLE( const void * rleData, int rleBlocks, int outBytes )
{
    char *out, *cp;

    struct RLEBlock {
        unsigned char count;
        unsigned char value;
    } * bp = (struct RLEBlock *) rleData;

    out = cp = (char *) malloc( outBytes );
    if ( out == NULL ) return NULL;

    while ( rleBlocks-- )
    {
        memset( cp, bp->value, bp->count );
        cp += bp->count;
        bp++;
    }

    return out;
}

//==========================================================================
// setVESAGraphicsMode

static int
setVESAGraphicsMode( unsigned short width,
                     unsigned short height,
                     unsigned char  bitsPerPixel,
                     unsigned short refreshRate )
{
    VBEModeInfoBlock  minfo;
    unsigned short    mode;
    unsigned short    vesaVersion;
    int               err = errFuncNotSupported;

    do {
        mode = getVESAModeWithProperties( width, height, bitsPerPixel,
                                          maColorModeBit             |
                                          maModeIsSupportedBit       |
                                          maGraphicsModeBit          |
                                          maLinearFrameBufferAvailBit,
                                          0,
                                          &minfo, &vesaVersion );
        if ( mode == modeEndOfList )
        {
            break;
        }

        if ( (vesaVersion >> 8) >= 3 && refreshRate >= 60 &&
             (gBootMode & kBootModeSafe) == 0 )
        {
            VBECRTCInfoBlock timing;
    
            // Generate CRTC timing for given refresh rate.

            generateCRTCTiming( minfo.XResolution, minfo.YResolution,
                                refreshRate, kCRTCParamRefreshRate,
                                &timing );

            // Find the actual pixel clock supported by the hardware.

            getVBEPixelClock( mode, &timing.PixelClock );

            // Re-compute CRTC timing based on actual pixel clock.

            generateCRTCTiming( minfo.XResolution, minfo.YResolution,
                                timing.PixelClock, kCRTCParamPixelClock,
                                &timing );

            // Set the video mode and use specified CRTC timing.

            err = setVBEMode( mode | kLinearFrameBufferBit |
                              kCustomRefreshRateBit, &timing );
        }
        else
        {
            // Set the mode with default refresh rate.

            err = setVBEMode( mode | kLinearFrameBufferBit, NULL );
        }
        if ( err != errSuccess )
        {
            break;
        }

        // Set 8-bit color palette.

        if ( minfo.BitsPerPixel == 8 )
        {
            VBEPalette palette;
            setupPalette( &palette, appleClut8 );
            if ((err = setVBEPalette(palette)) != errSuccess)
            {
                break;
            }
        }

        // Is this required for buggy Video BIOS implementations?
        // On which adapter?

        if ( minfo.BytesPerScanline == 0 )
             minfo.BytesPerScanline = ( minfo.XResolution *
                                        minfo.BitsPerPixel ) >> 3;

        // Update KernBootStruct using info provided by the selected
        // VESA mode.

        bootArgs->graphicsMode     = GRAPHICS_MODE;
        bootArgs->video.v_width    = minfo.XResolution;
        bootArgs->video.v_height   = minfo.YResolution;
        bootArgs->video.v_depth    = minfo.BitsPerPixel;
        bootArgs->video.v_rowBytes = minfo.BytesPerScanline;
        bootArgs->video.v_baseAddr = VBEMakeUInt32(minfo.PhysBasePtr);
    }
    while ( 0 );

    return err;
}

//==========================================================================
// drawBootGraphics

static int
drawBootGraphics( unsigned short width,
                  unsigned short height,
                  unsigned char  bitsPerPixel,
                  unsigned short refreshRate )
{
    VBEModeInfoBlock  minfo;
    unsigned short    mode;
    unsigned short    vesaVersion;
    int               err = errFuncNotSupported;

    char  * appleBoot = 0;
    short * appleBoot16;
    long  * appleBoot32;
    long    cnt, x, y;
    char  * appleBootPict;

    do {
        mode = getVESAModeWithProperties( width, height, bitsPerPixel,
                                          maColorModeBit             |
                                          maModeIsSupportedBit       |
                                          maGraphicsModeBit          |
                                          maLinearFrameBufferAvailBit,
                                          0,
                                          &minfo, &vesaVersion );
        if ( mode == modeEndOfList )
        {
            break;
        }

        // Fill the background to 75% grey (same as BootX).

        drawColorRectangle( 0, 0, minfo.XResolution, minfo.YResolution,
                            0x01 /* color index */ );

        appleBootPict = decodeRLE( gAppleBootPictRLE, kAppleBootRLEBlocks,
                                   kAppleBootWidth * kAppleBootHeight );

        // Prepare the data for the happy mac.

        if ( appleBootPict )
        {
            switch ( VIDEO(depth) )
            {
                case 16 :
                    appleBoot16 = malloc(kAppleBootWidth * kAppleBootHeight * 2);
                    if ( !appleBoot16 ) break;
                    for (cnt = 0; cnt < (kAppleBootWidth * kAppleBootHeight); cnt++)
                        appleBoot16[cnt] = lookUpCLUTIndex(appleBootPict[cnt], 16);
                    appleBoot = (char *) appleBoot16;
                    break;
    
                case 32 :
                    appleBoot32 = malloc(kAppleBootWidth * kAppleBootHeight * 4);
                    if ( !appleBoot32 ) break;
                    for (cnt = 0; cnt < (kAppleBootWidth * kAppleBootHeight); cnt++)
                        appleBoot32[cnt] = lookUpCLUTIndex(appleBootPict[cnt], 32);
                    appleBoot = (char *) appleBoot32;
                    break;
    
                default :
                    appleBoot = (char *) appleBootPict;
                    break;
            }

            x = ( VIDEO(width) - kAppleBootWidth ) / 2;
            y = ( VIDEO(height) - kAppleBootHeight ) / 2 + kAppleBootOffset;
    
            // Draw the happy mac in the center of the display.
            
            if ( appleBoot )
            {
                drawDataRectangle( x, y, kAppleBootWidth, kAppleBootHeight,
                                   appleBoot );
            }

            free( appleBootPict );
        }
    } while (0);

    return err;
}

//==========================================================================
// LookUpCLUTIndex

static unsigned long lookUpCLUTIndex( unsigned char index,
                                      unsigned char depth )
{
    long result, red, green, blue;
  
    red   = appleClut8[index * 3 + 0];
    green = appleClut8[index * 3 + 1];
    blue  = appleClut8[index * 3 + 2];

    switch (depth) {
        case 16 :
            result = ((red   & 0xF8) << 7) | 
                     ((green & 0xF8) << 2) |
                     ((blue  & 0xF8) >> 3);
            result |= (result << 16);
            break;

        case 32 :
            result = (red << 16) | (green << 8) | blue;
            break;

        default :
            result = index | (index << 8);
            result |= (result << 16);
            break;
    }

    return result;
}

//==========================================================================
// drawColorRectangle

static void * stosl(void * dst, long val, long len)
{
    asm( "rep; stosl"
       : "=c" (len), "=D" (dst)
       : "0" (len), "1" (dst), "a" (val)
       : "memory" );

    return dst;
}

static void drawColorRectangle( unsigned short x,
                                unsigned short y,
                                unsigned short width,
                                unsigned short height,
                                unsigned char  colorIndex )
{
    long   pixelBytes;
    long   color = lookUpCLUTIndex( colorIndex, VIDEO(depth) );
    char * vram;

    pixelBytes = VIDEO(depth) / 8;
    vram       = (char *) VIDEO(baseAddr) +
                 VIDEO(rowBytes) * y + pixelBytes * x;

    while ( height-- )
    {
        int rem = ( pixelBytes * width ) % 4;
        if ( rem ) bcopy( &color, vram, rem );
        stosl( vram + rem, color, pixelBytes * width / 4 );
        vram += VIDEO(rowBytes);
    }
}

//==========================================================================
// drawDataRectangle

static void drawDataRectangle( unsigned short  x,
                               unsigned short  y,
                               unsigned short  width,
                               unsigned short  height,
                               unsigned char * data )
{
    long   pixelBytes = VIDEO(depth) / 8;
    char * vram       = (char *) VIDEO(baseAddr) +
                        VIDEO(rowBytes) * y + pixelBytes * x;

    while ( height-- )
    {
        bcopy( data, vram, width * pixelBytes );
        vram += VIDEO(rowBytes);
        data += width * pixelBytes;
    }
}

//==========================================================================
// setVESATextMode

static int
setVESATextMode( unsigned short cols,
                 unsigned short rows,
                 unsigned char  bitsPerPixel )
{
    VBEModeInfoBlock  minfo;
    unsigned short    mode = modeEndOfList;

    if ( (cols != 80) || (rows != 25) )  // not 80x25 mode
    {
        mode = getVESAModeWithProperties( cols, rows, bitsPerPixel,
                                          maColorModeBit |
                                          maModeIsSupportedBit,
                                          maGraphicsModeBit,
                                          &minfo, NULL );
    }

    if ( ( mode == modeEndOfList ) || ( setVBEMode(mode, NULL) != errSuccess ) )
    {
        video_mode( 2 );  // VGA BIOS, 80x25 text mode.
        minfo.XResolution = 80;
        minfo.YResolution = 25;
    }

    // Update KernBootStruct using info provided by the selected
    // VESA mode.

    bootArgs->graphicsMode     = TEXT_MODE;
    bootArgs->video.v_baseAddr = 0xb8000;
    bootArgs->video.v_width    = minfo.XResolution;
    bootArgs->video.v_height   = minfo.YResolution;
    bootArgs->video.v_depth    = 8;
    bootArgs->video.v_rowBytes = 0x8000;

    return errSuccess;  // always return success
}

//==========================================================================
// getNumberArrayFromProperty

static int
getNumberArrayFromProperty( const char *  propKey,
                            unsigned long numbers[],
                            unsigned long maxArrayCount )
{
    char * propStr;
    unsigned long    count = 0;

#define _isdigit(c) ((c) >= '0' && (c) <= '9')

    propStr = newStringForKey( (char *) propKey );
    if ( propStr )
    {
        char * delimiter = propStr;
        char * p = propStr;

        while ( count < maxArrayCount && *p != '\0' )
        {
            unsigned long val = strtoul( p, &delimiter, 10 );
            if ( p != delimiter )
            {
                numbers[count++] = val;
                p = delimiter;
            }
            while ( ( *p != '\0' ) && !_isdigit(*p) )
                p++;
        }

        free( propStr );
    }

    return count;
}

//==========================================================================
// setVideoMode
//
// Set the video mode to TEXT_MODE or GRAPHICS_MODE.

void
setVideoMode( int mode )
{
    unsigned long params[4];
    int           count;
    int           err = errSuccess;

    if ( mode == GRAPHICS_MODE )
    {
        params[3] = 0;
        count = getNumberArrayFromProperty( kGraphicsModeKey, params, 4 );
        if ( count < 3 )
        {
            params[0] = 1024;  // Default graphics mode is 1024x768x16.
            params[1] = 768;
            params[2] = 16;
        }

        // Map from pixel format to bits per pixel.

        if ( params[2] == 256 ) params[2] = 8;
        if ( params[2] == 555 ) params[2] = 16;
        if ( params[2] == 888 ) params[2] = 32;

        err = setVESAGraphicsMode( params[0], params[1], params[2], params[3] );
        if ( err == errSuccess )
        {
            // If this boolean is set to true, then the console driver
            // in the kernel will show the animated color wheel on the
            // upper left corner.

            bootArgs->video.v_display = !gVerboseMode;
            
            if (!gVerboseMode) {
                drawBootGraphics( params[0], params[1], params[2], params[3] );
            }
        }
    }

    if ( (mode == TEXT_MODE) || (err != errSuccess) )
    {
        count = getNumberArrayFromProperty( kTextModeKey, params, 2 );
        if ( count < 2 )
        {
            params[0] = 80;  // Default text mode is 80x25.
            params[1] = 25;
        }

        setVESATextMode( params[0], params[1], 4 );
        bootArgs->video.v_display = 0;
    }

    currentIndicator = 0;
}

//==========================================================================
// Return the current video mode, TEXT_MODE or GRAPHICS_MODE.

int getVideoMode(void)
{
    return bootArgs->graphicsMode;
}

//==========================================================================
// Display and clear the activity indicator.

static char indicator[] = {'-', '\\', '|', '/', '-', '\\', '|', '/', '\0'};

// To prevent a ridiculously fast-spinning indicator,
// ensure a minimum of 1/9 sec between animation frames.
#define MIN_TICKS 2

void
spinActivityIndicator( void )
{
    static unsigned long lastTickTime = 0;
    unsigned long        currentTickTime = time18();
    static char          string[3] = {'\0', '\b', '\0'};
    
    if (currentTickTime < lastTickTime + MIN_TICKS)
        return;
    else
        lastTickTime = currentTickTime;

    if ( getVideoMode() == TEXT_MODE )
    {
        string[0] = indicator[currentIndicator];
        printf(string);
        if (indicator[++currentIndicator] == 0)
            currentIndicator = 0;
    }
}

void
clearActivityIndicator( void )
{
    if ( getVideoMode() == TEXT_MODE )
    {
        printf(" \b");
    }
}
