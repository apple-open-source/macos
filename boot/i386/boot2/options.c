/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2004 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
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

/* Flush keyboard buffer; returns TRUE if any of the flushed
 * characters was F8.
 */

static BOOL flushKeyboardBuffer()
{
    BOOL status = FALSE;

    while ( readKeyboardStatus() ) {
        if (bgetc() == 0x4200) status = TRUE;
    }
    return status;
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

        // Count can be interrupted by holding down shift,
        // control or alt key
        if ( ( readKeyboardShiftFlags() & 0x0F ) != 0 ) {
            ch = 1;
            break;
        }

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
        printf("Press Enter to start up the foreign OS. ");
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

static void
printMemoryInfo(void)
{
    int line;
    int i;
    MemoryRange *mp = bootArgs->memoryMap;

    // Activate and clear page 1
    setActiveDisplayPage(1);
    clearScreenRows(0, 24);
    setCursorPosition( 0, 0, 1 );

    printf("BIOS reported memory ranges:\n");
    line = 1;
    for (i=0; i<bootArgs->memoryMapCount; i++) {
        printf("Base 0x%08x%08x, ",
               (unsigned long)(mp->base >> 32),
               (unsigned long)(mp->base));
        printf("length 0x%08x%08x, type %d\n",
               (unsigned long)(mp->length >> 32),
               (unsigned long)(mp->length),
               mp->type);
        if (line++ > 20) {
            printf("(Press a key to continue...)");
            getc();
            line = 0;
        }
        mp++;
    }
    if (line > 0) {
        printf("(Press a key to continue...)");
        getc();
    }
    
    setActiveDisplayPage(0);
}

//==========================================================================

int
getBootOptions(BOOL firstRun)
{
    int     i;
    int     key;
    int     selectIndex = 0;
    int     bvCount;
    int     nextRow;
    int     timeout;
    BVRef   bvr;
    BVRef   bvChain;
    BVRef   menuBVR;
    BOOL    showPrompt, newShowPrompt;
    MenuItem *  menuItems = NULL;

    // Allow user to override default timeout.

    if ( getIntForKey(kTimeoutKey, &timeout) == NO )
    {
        timeout = kBootTimeout;
    }

    // If the user is holding down a shift key,
    // abort quiet mode.
    if ( ( readKeyboardShiftFlags() & 0x0F ) != 0 ) {
        gBootMode &= ~kBootModeQuiet;
    }

    // If user typed F8, abort quiet mode,
    // and display the menu.
    if (flushKeyboardBuffer()) {
        gBootMode &= ~kBootModeQuiet;
        timeout = 0;
    }

    clearBootArgs();

    setCursorPosition( 0, 0, 0 );
    clearScreenRows( 0, kScreenLastRow );
    if ( ! ( gBootMode & kBootModeQuiet ) ) {
        // Display banner and show hardware info. 
        printf( bootBanner, (bootArgs->convmem + bootArgs->extmem) / 1024 );
        printVBEInfo();
    }

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

    if ( gBootMode & kBootModeQuiet )
    {
        // No input allowed from user.
        goto done;
    }

    if ( firstRun && ( timeout > 0 ) &&
         ( countdown("Press any key to enter startup options.",
                     kMenuTopRow, timeout) == 0 ) )
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
            getBootVolumeDescription( bvr, menuItems[i].name, 80, YES );
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
                    if ( strcmp( gBootArgs, "?video" ) == 0 ) {
                        printVBEModeInfo();
                    } else if ( strcmp( gBootArgs, "?memory" ) == 0 ) {
                        printMemoryInfo();
                    } else {
                        showHelp();
                    }
                    key = 0;
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

    return 0;
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
    char *           argP;

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
    // in the config table, or use the default if not specified.
    // Specifying a kernel name on the command line, or specifying
    // a non-default kernel name in the config file counts as
    // overriding the kernel, which causes the kernelcache not
    // to be used.

    gOverrideKernel = NO;
    if (( kernel = extractKernelName((char **)&cp) )) {
        strcpy( bootArgs->bootFile, kernel );
        gOverrideKernel = YES;
    } else {
        if ( getValueForKey( kKernelNameKey, &val, &cnt ) ) {
            strlcpy( bootArgs->bootFile, val, cnt+1 );
            if (strcmp( bootArgs->bootFile, kDefaultKernel ) != 0) {
                gOverrideKernel = YES;
            }
        } else {
            strcpy( bootArgs->bootFile, kDefaultKernel );
        }
    }

    cntRemaining = BOOT_STRING_LEN - 2;  // save 1 for NULL, 1 for space

    // Check to see if we need to specify root device.
    // If user types "rd=.." on the boot line, it overrides
    // the boot device key in the boot arguments file.
    //
    argP = bootArgs->bootString;
    if ( getValueForBootKey( cp, kRootDeviceKey, &val, &cnt ) == FALSE &&
         getValueForKey( kRootDeviceKey, &val, &cnt ) == FALSE ) {
        if ( getValueForKey( kBootDeviceKey, &val, &cnt ) ) {
            strcpy( argP, "rd=*" );
            argP += 4;
            strlcpy( argP, val, cnt+1);
            cntRemaining -= cnt;
            argP += cnt;
            *argP++ = ' ';
        }
    }

    // Check to see if we should ignore saved kernel flags.
    if (getValueForBootKey(cp, kIgnoreBootFileFlag, &val, &cnt) == FALSE) {
        if (getValueForKey( kKernelFlagsKey, &val, &cnt ) == FALSE) {
	    val = 0;
	    cnt = 0;
        }
    }

    // Store the merged kernel flags and boot args.

    if (cnt > cntRemaining) {
	error("Warning: boot arguments too long, truncated\n");
	cnt = cntRemaining;
    }
    if (cnt) {
      strncpy(argP, val, cnt);
      argP[cnt++] = ' ';
    }
    cntRemaining = cntRemaining - cnt;
    userCnt = strlen(cp);
    if (userCnt > cntRemaining) {
	error("Warning: boot arguments too long, truncated\n");
	userCnt = cntRemaining;
    }
    strncpy(&argP[cnt], cp, userCnt);
    argP[cnt+userCnt] = '\0';

    gVerboseMode = getValueForKey( kVerboseModeFlag, &val, &cnt ) ||
                   getValueForKey( kSingleUserModeFlag, &val, &cnt );

    gBootMode = ( getValueForKey( kSafeModeFlag, &val, &cnt ) ) ?
                kBootModeSafe : kBootModeNormal;

    if ( getValueForKey( kPlatformKey, &val, &cnt ) ) {
        strlcpy(gPlatformName, val, cnt + 1);
    } else {
        strcpy(gPlatformName, "ACPI");
    }

    if ( getValueForKey( kMKextCacheKey, &val, &cnt ) ) {
        strlcpy(gMKextName, val, cnt + 1);
    }

    return 0;
}

//==========================================================================
// Load the help file and display the file contents on the screen.

static void showHelp()
{
#define BOOT_HELP_PATH  "/usr/standalone/i386/BootHelp.txt"

    int  fd;
    int  size;
    int  line;
    int  line_offset;
    int  c;

    if ( (fd = open(BOOT_HELP_PATH, 0)) >= 0 )
    {
        char * buffer;
        char * bp;

        size = file_size(fd);
        buffer = malloc( size + 1 );
        read(fd, buffer, size);
        close(fd);

        bp = buffer;
        while (size > 0) {
            while (*bp != '\n') {
            bp++;
            size--;
          }
          *bp++ = '\0';
          size--;
        }
        *bp = '\1';
        line_offset = 0;

        setActiveDisplayPage(1);

        while (1) {
            clearScreenRows(0, 24);
            setCursorPosition(0, 0, 1);
            bp = buffer;
            for (line = 0; *bp != '\1' && line < line_offset; line++) {
                while (*bp != '\0') bp++;
                bp++;
            }
            for (line = 0; *bp != '\1' && line < 23; line++) {
                setCursorPosition(0, line, 1);
                printf("%s\n", bp);
                while (*bp != '\0') bp++;
                bp++;
            }

            setCursorPosition(0, 23, 1);
            if (*bp == '\1') {
                printf("[Type %sq or space to quit help]",
                       (line_offset > 0) ? "p for previous page, " : "");
            } else {
                printf("[Type %s%sq to quit help]",
                       (line_offset > 0) ? "p for previous page, " : "",
                       (*bp != '\1') ? "space for next page, " : "");
            }

            c = getc();
            if (c == 'q' || c == 'Q') {
                break;
            }
            if ((c == 'p' || c == 'P') && line_offset > 0) {
                line_offset -= 23;
            }
            if (c == ' ') {
                if (*bp == '\1') {
                    break;
                } else {
                    line_offset += 23;
                }
            }
        }

        free(buffer);
        setActiveDisplayPage(0);
    }
}
