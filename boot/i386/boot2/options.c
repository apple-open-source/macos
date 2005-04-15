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

#include "boot.h"
#include "bootstruct.h"
#include "fdisk.h"

enum {
    kReturnKey     = 0x0d,
    kEscapeKey     = 0x1b,
    kBackspaceKey  = 0x08,
    kASCIIKeyMask  = 0x7f
};

enum {
    kMenuTopRow    = 5,
    kMenuMaxItems  = 6,
    kScreenLastRow = 24
};

static BVRef gBootVolume = 0;

static void showHelp();

//==========================================================================

enum {
    kCursorTypeHidden    = 0x0100,
    kCursorTypeUnderline = 0x0607
};

typedef struct {
    int x;
    int y;
    int type;
} CursorState;

static void changeCursor( int col, int row, int type, CursorState * cs )
{
    if (cs) getCursorPositionAndType( &cs->x, &cs->y, &cs->type );
    setCursorType( type );
    setCursorPosition( col, row, 0 );
}

static void moveCursor( int col, int row )
{
    setCursorPosition( col, row, 0 );
}

static void restoreCursor( const CursorState * cs )
{
    setCursorPosition( cs->x, cs->y, 0 );
    setCursorType( cs->type );
}

//==========================================================================

static void flushKeyboardBuffer()
{
    while ( readKeyboardStatus() ) getc();
}

//==========================================================================

static int countdown( const char * msg, int row, int timeout )
{
    unsigned long time;
    int ch  = 0;
    int col = strlen(msg) + 1;

    flushKeyboardBuffer();

    moveCursor( 0, row );
    printf(msg);

    for ( time = time18(), timeout++; timeout; )
    {
        if (ch = readKeyboardStatus())
            break;

        if ( time18() >= time )
        {
            time += 18;
            timeout--;
            moveCursor( col, row );
            printf("(%d) ", timeout);
        }
    }

    flushKeyboardBuffer();

    return ch;
}

//==========================================================================

static char   gBootArgs[BOOT_STRING_LEN];
static char * gBootArgsPtr = gBootArgs;
static char * gBootArgsEnd = gBootArgs + BOOT_STRING_LEN - 1;

static void clearBootArgs()
{
    gBootArgsPtr = gBootArgs;
    memset( gBootArgs, '\0', BOOT_STRING_LEN );
}

//==========================================================================

static void showBootPrompt( int row, BOOL visible )
{
    extern char bootPrompt[];

    changeCursor( 0, row, kCursorTypeUnderline, 0 );    
    clearScreenRows( row, kScreenLastRow );
    clearBootArgs();

    if ( visible )
    {
        printf( bootPrompt );
    }
    else
    {
        printf("Press Return to start up the foreign OS. ");
    }
}

//==========================================================================

static void updateBootArgs( int key )
{
    key &= kASCIIKeyMask;

    switch ( key )
    {
        case kBackspaceKey:
            if ( gBootArgsPtr > gBootArgs )
            {
                int x, y, t;
                getCursorPositionAndType( &x, &y, &t );
                if ( x == 0 && y )
                {
                    x = 80; y--;
                }
                if (x) x--;
                setCursorPosition( x, y, 0 );
                putca(' ', 0x07, 1);
                *gBootArgsPtr-- = '\0';
            }
            break;

        default:
            if ( key >= ' ' && gBootArgsPtr < gBootArgsEnd)
            {
                putchar(key);  // echo to screen
                *gBootArgsPtr++ = key;
            }
            break;
    }
}

//==========================================================================

typedef struct {
    char   name[80];
    void * param;
} MenuItem;

static const MenuItem * gMenuItems = NULL;
static int   gMenuItemCount;
static int   gMenuRow;
static int   gMenuHeight;
static int   gMenuTop;
static int   gMenuBottom;
static int   gMenuSelection;

static void printMenuItem( const MenuItem * item, int highlight )
{
    printf("  ");

    if ( highlight )
        putca(' ', 0x70, strlen(item->name) + 4);
    else
        putca(' ', 0x07, 40);

    printf("  %40s\n", item->name);
}

//==========================================================================

static void showMenu( const MenuItem * items, int count,
                      int selection, int row, int height )
{
    int         i;
    CursorState cursorState;

    if ( items == NULL || count == 0 ) return;

    // head and tail points to the start and the end of the list.
    // top and bottom points to the first and last visible items
    // in the menu window.

    gMenuItems     = items;
    gMenuRow       = row;
    gMenuHeight    = height;
    gMenuItemCount = count;
    gMenuTop       = 0;
    gMenuBottom    = min( count, height ) - 1;
    gMenuSelection = selection;

    // If the selected item is not visible, shift the list down.

    if ( gMenuSelection > gMenuBottom )
    {
        gMenuTop += ( gMenuSelection - gMenuBottom );
        gMenuBottom = gMenuSelection;
    }

    // Draw the visible items.

    changeCursor( 0, row, kCursorTypeHidden, &cursorState );

    for ( i = gMenuTop; i <= gMenuBottom; i++ )
    {
        printMenuItem( &items[i], (i == gMenuSelection) );
    }

    restoreCursor( &cursorState );
}

//==========================================================================

static int updateMenu( int key, void ** paramPtr )
{
    int moved = 0;

    union {
        struct {
            unsigned int
                selectionUp   : 1,
                selectionDown : 1,
                scrollUp      : 1,
                scrollDown    : 1;
        } f;
        unsigned int w;
    } draw = {{0}};

    if ( NULL == gMenuItems ) return 0;

    // Look at the scan code.

    switch ( key )
    {
        case 0x4800:  // Up Arrow
            if ( gMenuSelection != gMenuTop )
                draw.f.selectionUp = 1;
            else if ( gMenuTop > 0 )
                draw.f.scrollDown = 1;
            break;

        case 0x5000:  // Down Arrow
            if ( gMenuSelection != gMenuBottom )
                draw.f.selectionDown = 1;
            else if ( gMenuBottom < (gMenuItemCount - 1) ) 
                draw.f.scrollUp = 1;
            break;
    }

    if ( draw.w )
    {
        if ( draw.f.scrollUp )
        {
            scollPage(0, gMenuRow, 40, gMenuRow + gMenuHeight - 1, 0x07, 1, 1);
            gMenuTop++; gMenuBottom++;
            draw.f.selectionDown = 1;
        }

        if ( draw.f.scrollDown )
        {
            scollPage(0, gMenuRow, 40, gMenuRow + gMenuHeight - 1, 0x07, 1, -1);
            gMenuTop--; gMenuBottom--;
            draw.f.selectionUp = 1;
        }

        if ( draw.f.selectionUp || draw.f.selectionDown )
        {
            CursorState cursorState;

            // Set cursor at current position, and clear inverse video.

            changeCursor( 0, gMenuRow + gMenuSelection - gMenuTop,
                          kCursorTypeHidden, &cursorState );

            printMenuItem( &gMenuItems[gMenuSelection], 0 );

            if ( draw.f.selectionUp ) gMenuSelection--;
            else                      gMenuSelection++;

            moveCursor( 0, gMenuRow + gMenuSelection - gMenuTop );

            printMenuItem( &gMenuItems[gMenuSelection], 1 );

            restoreCursor( &cursorState );
        }

        *paramPtr = gMenuItems[gMenuSelection].param;        
        moved = 1;
    }

    return moved;
}

//==========================================================================

static void skipblanks( const char ** cpp ) 
{
    while ( **(cpp) == ' ' || **(cpp) == '\t' ) ++(*cpp);
}

//==========================================================================

static const char * extractKernelName( char ** cpp )
{
    char * kn = *cpp;
    char * cp = *cpp;
    char   c;

    // Convert char to lower case.

    c = *cp | 0x20;

    // Must start with a letter or a '/'.

    if ( (c < 'a' || c > 'z') && ( c != '/' ) )
        return 0;

    // Keep consuming characters until we hit a separator.

    while ( *cp && (*cp != '=') && (*cp != ' ') && (*cp != '\t') )
        cp++;

    // Only SPACE or TAB separator is accepted.
    // Reject everything else.

    if (*cp == '=')
        return 0;

    // Overwrite the separator, and move the pointer past
    // the kernel name.

    if (*cp != '\0') *cp++ = '\0';
    *cpp = cp;

    return kn;
}

//==========================================================================

void getBootOptions()
{
    int     i;
    int     key;
    int     selectIndex = 0;
    int     bvCount;
    int     nextRow;
    BVRef   bvr;
    BVRef   bvChain;
    BVRef   menuBVR;
    BOOL    showPrompt, newShowPrompt;
    MenuItem *  menuItems = NULL;
    static BOOL firstRun  = YES;

    clearBootArgs();
    clearScreenRows( kMenuTopRow, kScreenLastRow );
    changeCursor( 0, kMenuTopRow, kCursorTypeUnderline, 0 );
    verbose("Scanning device %x...", gBIOSDev);

    // Get a list of bootable volumes on the device.

    bvChain = scanBootVolumes( gBIOSDev, &bvCount );
    gBootVolume = menuBVR = selectBootVolume( bvChain );

#if 0
    // When booting from CD (via HD emulation), default to hard
    // drive boot when possible. 

    if ( gBootVolume->part_type == FDISK_BOOTER &&
         gBootVolume->biosdev   == 0x80 )
    {
        // Scan the original device 0x80 that has been displaced
        // by the CD-ROM.

        BVRef hd_bvr = selectBootVolume(scanBootVolumes(0x81, 0));
        if ( hd_bvr->flags & kBVFlagNativeBoot )
        {
            int key = countdown("Press C to start up from CD-ROM.",
                                kMenuTopRow, 5);
            
            if ( (key & 0x5f) != 'c' )
            {
                gBootVolume = hd_bvr;
                gBIOSDev = hd_bvr->biosdev;
                initKernBootStruct( gBIOSDev );
                goto done;
            }
        }
    }
#endif

    // Allow user to override default setting.

    if ( firstRun &&
         countdown("Press any key to enter startup options.",
                   kMenuTopRow, 3) == 0 )
    {
        goto done;
    }

    if ( bvCount )
    {
        // Allocate memory for an array of menu items.

        menuItems = (MenuItem *) malloc( sizeof(MenuItem) * bvCount );
        if ( menuItems == NULL ) goto done;

        // Associate a menu item for each BVRef.

        for ( bvr = bvChain, i = bvCount - 1, selectIndex = 0;
              bvr; bvr = bvr->next, i-- )
        {
            getBootVolumeDescription( bvr, menuItems[i].name, 80 );
            menuItems[i].param = (void *) bvr;
            if ( bvr == menuBVR ) selectIndex = i;
        }
    }

    // Clear screen and hide the blinking cursor.

    clearScreenRows( kMenuTopRow, kMenuTopRow + 2 );
    changeCursor( 0, kMenuTopRow, kCursorTypeHidden, 0 );
    nextRow = kMenuTopRow;
    showPrompt = YES;

    // Show the menu.

    if ( bvCount )
    {
        printf("Use \30\31 keys to select the startup volume.");
        showMenu( menuItems, bvCount, selectIndex, kMenuTopRow + 2, kMenuMaxItems );
        nextRow += min( bvCount, kMenuMaxItems ) + 3;
    }

    // Show the boot prompt.

    showPrompt = (bvCount == 0) || (menuBVR->flags & kBVFlagNativeBoot);
    showBootPrompt( nextRow, showPrompt );

    do {
        key = getc();

        updateMenu( key, (void **) &menuBVR );

        newShowPrompt = (bvCount == 0) ||
                        (menuBVR->flags & kBVFlagNativeBoot);

        if ( newShowPrompt != showPrompt )
        {
            showPrompt = newShowPrompt;
            showBootPrompt( nextRow, showPrompt );
        }
        if ( showPrompt ) updateBootArgs( key );

        switch ( key & kASCIIKeyMask )
        {
            case kReturnKey:
                if ( *gBootArgs == '?' )
                {
                    showHelp(); key = 0;
                    showBootPrompt( nextRow, showPrompt );
                    break;
                }
                gBootVolume = menuBVR;
                break;

            case kEscapeKey:
                clearBootArgs();
                break;

            default:
                key = 0;
        }
    }
    while ( 0 == key );

done:
    firstRun = NO;

    clearScreenRows( kMenuTopRow, kScreenLastRow );
    changeCursor( 0, kMenuTopRow, kCursorTypeUnderline, 0 );

    if ( menuItems ) free(menuItems);
}

//==========================================================================

extern unsigned char chainbootdev;
extern unsigned char chainbootflag;

int processBootOptions()
{
    const char *     cp  = gBootArgs;
    const char *     val = 0;
    const char *     kernel;
    int              cnt;
    int		     userCnt;
    int              cntRemaining;

    skipblanks( &cp );

    // Update the unit and partition number.

    if ( gBootVolume )
    {
        if ( gBootVolume->flags & kBVFlagForeignBoot )
        {
            readBootSector( gBootVolume->biosdev, gBootVolume->part_boff,
                            (void *) 0x7c00 );

            //
            // Setup edx, and signal intention to chain load the
            // foreign booter.
            //

            chainbootdev  = gBootVolume->biosdev;
            chainbootflag = 1;

            return 1;
        }

        bootArgs->kernDev &= ~((B_UNITMASK << B_UNITSHIFT ) |
                          (B_PARTITIONMASK << B_PARTITIONSHIFT));

        bootArgs->kernDev |= MAKEKERNDEV(    0,
 		         /* unit */      BIOS_DEV_UNIT(gBootVolume),
                        /* partition */ gBootVolume->part_no );
    }

    // Load config table specified by the user, or use the default.

    if (getValueForBootKey( cp, "config", &val, &cnt ) == FALSE) {
	val = 0;
	cnt = 0;
    }
    loadSystemConfig(val, cnt);
    if ( !sysConfigValid ) return -1;

    // Use the kernel name specified by the user, or fetch the name
    // in the config table.

    if (( kernel = extractKernelName((char **)&cp) ))
    {
        strcpy( bootArgs->bootFile, kernel );
    }
    else
    {
        if ( getValueForKey( kKernelNameKey, &val, &cnt ) )
            strlcpy( bootArgs->bootFile, val, cnt+1 );
    }

    // Check to see if we should ignore saved kernel flags.
    if (getValueForBootKey(cp, "-F", &val, &cnt) == FALSE) {
        if (getValueForKey( kKernelFlagsKey, &val, &cnt ) == FALSE) {
	    val = 0;
	    cnt = 0;
        }
    }

    // Store the merged kernel flags and boot args.

    cntRemaining = BOOT_STRING_LEN - 2;  // save 1 for NULL, 1 for space
    if (cnt > cntRemaining) {
	error("Warning: boot arguments too long, truncated\n");
	cnt = cntRemaining;
    }
    if (cnt) {
      strncpy(bootArgs->bootString, val, cnt);
      bootArgs->bootString[cnt++] = ' ';
    }
    cntRemaining = cntRemaining - cnt;
    userCnt = strlen(cp);
    if (userCnt > cntRemaining) {
	error("Warning: boot arguments too long, truncated\n");
	userCnt = cntRemaining;
    }
    strncpy(&bootArgs->bootString[cnt], cp, userCnt);
    bootArgs->bootString[cnt+userCnt] = '\0';

    gVerboseMode = getValueForKey( "-v", &val, &cnt ) ||
                   getValueForKey( "-s", &val, &cnt );

    gBootGraphics = getBoolForKey( kBootGraphicsKey );

    gBootGraphics = YES;
    if ( getValueForKey(kBootGraphicsKey, &val, &cnt) && cnt &&
         (val[0] == 'N' || val[0] == 'n') )
        gBootGraphics = NO;

    gBootMode = ( getValueForKey( "-f", &val, &cnt ) ) ?
                kBootModeSafe : kBootModeNormal;

    return 0;
}

//==========================================================================
// Load the help file and display the file contents on the screen.

static void showHelp()
{
#define BOOT_HELP_PATH  "/usr/standalone/i386/BootHelp.txt"

    int  fd;

    if ( (fd = open(BOOT_HELP_PATH, 0)) >= 0 )
    {
        char * buffer;

        // Activate and clear page 1
        // Perhaps this should be loaded only once?

        setActiveDisplayPage(1);
        clearScreenRows(0, 24);
        setCursorPosition( 0, 0, 1 );
        
        buffer = malloc( file_size(fd) );
        read(fd, buffer, file_size(fd) - 1);
        close(fd);
        printf("%s", buffer);
        free(buffer);
        
        // Wait for a keystroke and return to page 0.

        getc();
        setActiveDisplayPage(0);
    }
}
