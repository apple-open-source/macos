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
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#include "libsaio.h"
#include "boot.h"
#include "vbe.h"
#include "kernBootStruct.h"

#define CHAR_W         8
#define CHAR_W_SHIFT   3
#define CHAR_H         16
#define CHAR_H_SHIFT   4
#define BYTE_SHIFT     3
#define NCOLS          (screen_width / CHAR_W)
#define NROWS          (screen_height / CHAR_H)
#define SCREEN_W       (screen_width)
#define SCREEN_H       (screen_height)

/*
 * Forward declarations.
 */
static int convert_vbe_mode(char * mode_name, int * mode);
BOOL gSilentBoot;

//==========================================================================
// Display a (optionally centered) text message.

void
message(char * str, int centered)
{
	register int x;

	x = (NCOLS - strlen(str)) >> 1;

 	if ( currentMode() == TEXT_MODE )
    {
	    if (centered)
        {
            while(x--) printf(" ");
        }
	    printf("%s\n", str);
	}
}

/*
 * for spinning disk
 */
static int currentIndicator = 0;

//==========================================================================
// Set the screen mode: TEXT_MODE or GRAPHICS_MODE

void
setMode(int mode)
{
	unsigned int vmode;
	char *       vmode_name;
    int          err = errSuccess;

    if ( currentMode() == mode ) return;

    if ( mode == GRAPHICS_MODE &&
        (vmode_name = newStringForKey(kGraphicsModeKey)) != 0)
    {
        // Set to the graphics mode specified in the config table file,
        // enable linear frame buffer mode, and update kernBootStruct.

        if ( convert_vbe_mode(vmode_name, &vmode) == 0 )
            vmode = mode1024x768x256;   /* default mode */

        err = set_linear_video_mode(vmode);

        if ( err == errSuccess )
        {
            kernBootStruct->graphicsMode     = GRAPHICS_MODE;
            kernBootStruct->video.v_display  = gSilentBoot;
            kernBootStruct->video.v_baseAddr = (unsigned long) frame_buffer;
            kernBootStruct->video.v_width    = SCREEN_W;
            kernBootStruct->video.v_height   = SCREEN_H;
            kernBootStruct->video.v_depth    = bits_per_pixel;
            kernBootStruct->video.v_rowBytes = (screen_rowbytes == 0) ? 
                (SCREEN_W * bits_per_pixel) >> BYTE_SHIFT : screen_rowbytes;
        }

        free(vmode_name);
    }
    
    if ( (mode == TEXT_MODE) || (err != errSuccess) )
    {
        video_mode(2);  // 80x25 text mode

        kernBootStruct->graphicsMode     = TEXT_MODE;
        kernBootStruct->video.v_display  = 0;
        kernBootStruct->video.v_baseAddr = (unsigned long) 0xb8000;
        kernBootStruct->video.v_width    = 80;
        kernBootStruct->video.v_height   = 25;
        kernBootStruct->video.v_depth    = 8;
        kernBootStruct->video.v_rowBytes = 0x8000;
    }

	currentIndicator = 0;
}

//==========================================================================
// Return the current screen mode, TEXT_MODE or GRAPHICS_MODE.

int currentMode(void)
{
    return kernBootStruct->graphicsMode;
}

//==========================================================================
// Convert from a string describing a graphics mode, to a VGA mode number.

typedef struct {
    char mode_name[15];
    int  mode_val;
} mode_table_t;

mode_table_t mode_table[] = {
    { "640x400x256",   mode640x400x256   },
    { "640x480x256",   mode640x480x256   },
    { "800x600x16",    mode800x600x16    },
    { "800x600x256",   mode800x600x256   },
    { "1024x768x16",   mode1024x768x16   },
    { "1024x768x256",  mode1024x768x256  },
    { "1280x1024x16",  mode1280x1024x16  },
    { "1280x1024x256", mode1280x1024x256 },
    { "640x480x555",   mode640x480x555   },
    { "640x480x888",   mode640x480x888   },
    { "800x600x555",   mode800x600x555   },
    { "800x600x888",   mode800x600x888   },
    { "1024x768x555",  mode1024x768x555  },
    { "1024x768x888",  mode1024x768x888  },
    { "1280x1024x555", mode1280x1024x555 },
    { "1280x1024x888", mode1280x1024x888 },
    { "", 0 }
};

int convert_vbe_mode(char * mode_name, int * mode)
{
    mode_table_t * mtp = mode_table;

    if (mode_name == 0 || *mode_name == 0)
        return 0;

    while ( *mtp->mode_name )
    {
        if (strcmp(mtp->mode_name, mode_name) == 0)
        {
	        *mode =  mtp->mode_val;
	        return 1;
        }
        mtp++;
    }
    return 0;
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
	
    if ( currentMode() == TEXT_MODE )
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
    if ( currentMode() == TEXT_MODE )
    {
        printf(" \b");
    }
}
