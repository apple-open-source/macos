/*
 * xf86Aiptek
 *
 * Lineage: This driver is based on both the xf86HyperPen and xf86Wacom tablet
 *          drivers.
 *
 * xf86HyperPen -- modified from xf86Summa (c) 1996 Steven Lang
 *  (c) 2000 Roland Jansen
 *  (c) 2000 Christian Herzog (button & 19200 baud support)
 *
 * xf86Wacom -- (c) 1995-2001 Frederic Lepied
 *
 * This driver assumes Linux HID support, available for USB devices.
 * 
 * Version 0.0, 1-Jan-2003, Bryan W. Headley
 * 
 * Copyright 2003 by Bryan W. Headley. <bwheadley@earthlink.net>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Bryan W. Headley not be used in 
 * advertising or publicity pertaining to distribution of the software 
 * without specific, written prior permission.  Bryan W. Headley makes no 
 * representations about the suitability of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
 *
 * BRYAN W. HEADLEY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL BRYAN W. HEADLEY BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTIONS, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/aiptek/xf86Aiptek.h,v 1.3 2003/12/10 01:50:33 dawes Exp $ */

#ifndef _AIPTEK_H_
#define _AIPTEK_H_


#ifdef LINUX_INPUT
#   include <asm/types.h>
#   include <linux/input.h>
#   ifndef EV_MSC
#	define EV_MSC 0x04
#   endif
/* keithp - a hack to avoid redefinitions of these in xf86str.h */
#   ifdef BUS_PCI
#       undef BUS_PCI
#   endif
#   ifdef BUS_ISA
#       undef BUS_ISA
#   endif
#endif

#include <xf86Version.h>

#ifndef XFree86LOADER
#   include <unistd.h>
#   include <errno.h>
#endif

#include <misc.h>
#include <xf86.h>

#define NEED_XF86_TYPES

#if !defined(DGUX)
#   include <xf86_ansic.h>
#   include <xisb.h>
#endif

#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <exevents.h>        /* Needed for InitValuator/Proximity stuff */
#include <keysym.h>
#include <mipointer.h>

#ifdef XFree86LOADER
#   include <xf86Module.h>
#endif

#define XCONFIG_PROBED "(==)"
#define XCONFIG_GIVEN  "(**)"

#define xf86Verbose 1

#undef PRIVATE
#define PRIVATE(x) XI_PRIVATE(x)

#define CURSOR_SECTION_NAME "AiptekCursor"
#define STYLUS_SECTION_NAME "AiptekStylus"
#define ERASER_SECTION_NAME "AiptekEraser"

#define XI_STYLUS           "STYLUS"    /* X device name for the stylus */
#define XI_CURSOR           "CURSOR"    /* X device name for the cursor */
#define XI_ERASER           "ERASER"    /* X device name for the eraser */

/* macro from counts/inch to counts/meter */
#define LPI2CPM(res)    (res * 1000 / 25.4)

/* max number of input events to read in one read call */
#define MAX_EVENTS 50

#define BITS_PER_LONG           (sizeof(long) * 8)
#define NBITS(x)                ((((x)-1)/BITS_PER_LONG)+1)
#define TEST_BIT(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)
#define OFF(x)                  ((x)%BITS_PER_LONG)
#define LONG(x)                 ((x)/BITS_PER_LONG)

/******************************************************************************
 * Debugging macros
 */
#ifdef DBG
#   undef DBG
#endif

#ifdef DEBUG
#   undef DEBUG
#endif

#ifndef INI_DEBUG_LEVEL
#   define INI_DEBUG_LEVEL 0
#endif

#define DEBUG 1
#if DEBUG
#   define     DBG(lvl, f)     {if ((lvl) <= debug_level) f;}
#else
#   define     DBG(lvl, f)
#endif

/******************************************************************************
 * AiptekDeviceRec flag bitmasks
 */
#define DEVICE_ID(flags) ((flags) & 0x07)

#define STYLUS_ID               0x01
#define CURSOR_ID               0x02
#define ERASER_ID               0x04

#define INVX_FLAG               0x08
#define INVY_FLAG               0x10

#define FIRST_TOUCH_FLAG        0x20
#define ABSOLUTE_FLAG           0x40
#define KEEP_SHAPE_FLAG         0x80

#define DEFAULT_BTN_TOUCH_THRESHOLD 100

/******************************************************************************
 * AiptekDeviceState 'buttons' bitmasks
 */

    /* Stylus events */
#define BUTTONS_EVENT_TOUCH         0x01
#define BUTTONS_EVENT_STYLUS        0x02
#define BUTTONS_EVENT_STYLUS2       0x04
#define BUTTONS_EVENT_SIDE_BTN      0x08
#define BUTTONS_EVENT_EXTRA_BTN     0x10

    /* Mouse events */
#define BUTTONS_EVENT_MOUSE_LEFT    0x01
#define BUTTONS_EVENT_MOUSE_RIGHT   0x02
#define BUTTONS_EVENT_MOUSE_MIDDLE  0x04

/******************************************************************************
 * AiptekDeviceRec 'zMode' settings
 */
#define PRESSURE_MODE_LINEAR        0
#define PRESSURE_MODE_SOFT_SMOOTH   1
#define PRESSURE_MODE_HARD_SMOOTH   2

/******************************************************************************
 * Used throughout to indicate numeric options
 * whose value has not been set (e.g., do not enable it's functionality,
 * or resort to default behavior).
 */

#define VALUE_NA     -1

/*****************************************************************************
 * AiptekDeviceState is used to contain the 'record' of events read
 * from the tablet. In theory, all of these events are collected
 * prior to a report to XInput. In practice, some of these won't be
 * known.
 */
typedef struct
{
    int     eventType;      /* STYLUS_ID, CURSOR_ID, ERASER_ID */
    int     x;              /* X value read */
    int     y;              /* Y value read */
    int     z;              /* Z value read */
    int     xTilt;          /* Angle at which stylus is held, X coord */
    int     yTilt;          /* Angle at which stylus is held, Y coord */
    int     proximity;      /* Stylus proximity bit. */
    int     macroKey;       /* Macrokey read from tablet */
    int     button;         /* Button bitmask */
    int     distance;       /* Future capacity */
    int     wheel;          /* Future capacity */
} AiptekStateRec, *AiptekStatePtr;

/*****************************************************************************
 * AiptekDeviceRec contains information on how a physical device 
 * or pseudo-device is configured. Latter needs explanation: a tablet can
 * act as an input device with, say, a stylus, a cursor/puck, and
 * an eraser. The physical device is the Aiptek; but for purposes of inane
 * fun, the stylus, cursor/puck, and the eraser are considered to
 * be separate 'pseudo' devices. What this means is that you might set
 * the stylus to only work in a subset of the active area; the cursor/puck
 * may have access to the entire region, and the eraser has it's X & Y
 * coordinates inverted.
 *
 * Second point: the cursor device is also known as a 'puck'. Aiptek though
 * gives us a mouse to serve as our puck/cursor. But to keep our sanity,
 * we refer throughout this driver as either stylus or puck, as those are
 * Input Tablet terms. Hmmph. :-)
 */

typedef struct
{
    /* configuration fields */
    unsigned char   flags;          /* we keep device type, 
                                     * absolute | relative coordinates,
                                     * firstTouch bits inside. */

    /* If unspecified, we will set xSize and ySize to match the
     * active area of the tablet, and also set xOffset and yOffset to 0.
     *
     * Otherwise: this documents an active area of the tablet, outside
     * of whose coordinates input is ignored. Size is computed relative
     * to upper-left corner, known as 0,0.
     *
     * Now, to that upper-left corner, xOffset and yOffset may be
     * applied, which moves the origin coordinate right and down,
     * respectively.
     */
    int             xSize;          /* Active area X */
    int             ySize;          /* Active area Y */
    int             xOffset;        /* Active area offset X */
    int             yOffset;        /* Active area offset Y */

    /* Maximum size of the tablet. Presumed to be the size of
     * the tablet drawing area if unspecified; can be used
     * to define a cut-off point from where on the tablet 
     * input will be ignored. Yes, very synonymous with 'active area',
     * above. Just a different way of expressing the same concept,
     * except there is no xOffset/yOffset parameters.
     */

    int             xMax;           /* Maximum X value */
    int             yMax;           /* Maximum Y value */

    /* Limit the range of pressure values we will accept
     * as input. Note that throughout, coordinate 'Z' refers
     * to stylus pen pressure.
     *
     * Also, note that zMin/zMax is different than zThreshold.
     */
    int             zMin;           /* Minimum Z value */
    int             zMax;           /* Maximum Z value */

    /* Changing xTop/xBottom/yTop/yBottom affects the resolution 
     * in points/inch for that given axis.
     */
    int             xTop;
    int             yTop;
    int             xBottom;
    int             yBottom;

    /* Threshold allows the user to specify a minimal amount the device
     * has to move in a given direction before the input is accepted.
     * The benefits here are to help eliminate garbage input from unsteady
     * hands/tool.
     */
    int             xThreshold;     /* accept X report if delta > threshold */ 
    int             yThreshold;     /* accept Y report if delta > threshold */
    int             zThreshold;     /* accept pressure if delta > threshold */
    int             xTiltThreshold; /* accept xTilt if delta > threshold */
    int             yTiltThreshold; /* accept yTilt if delta > threshold */
    /* We provide the Z coordinate either in linear or log mode.
     * The nomenclature "log" is fairly generic, except that
     * our smoothing algorithms are based on sqrt(). We support,
     * "Soft"
     *          z = z * z / maxPressure (512)
     * "Hard"
     *          z = maxPressure * sqrt( z / maxPressure )
     */    
    int             zMode;          /* Z reported linearly or 'smoothed' */

    /* This is magic pixie dust, and no, I don't know why it's here, and
     * yes, I want to remove it, and no, not yet, but yes, soon. TODO!
     */ 
    int             initNumber;     /* magic number for the init phasis */

    /* By default, the tablet runs in window :0.0. However, you can change
     * things such that it only supports screen 1, or whatever. I know what
     * you are thinking: xinerama with two tablets. I am resisting you.
     */
    int             screenNo;       /* associated screen */

    /* Previous state values. Allows us to filter out getting the same
     * tablet report over and over again.
     */


    /* This struct is used to keep parameters, etc., that
     * are common between ALL pseudo-devices of the aiptek
     * driver. E.g., the physical size of the drawing
     * area is kept here.
     */
    struct _AiptekCommonRec* common;/* common info pointer */

} AiptekDeviceRec, *AiptekDevicePtr;


typedef struct _AiptekCommonRec 
{
    char*           deviceName;     /* device file name */
    unsigned char   flags;          /* various flags (handle tilt) */

    /* Same goddamn magic pixie dust thingie that's also in
     * AiptekDeviceRec. I MUST get rid of this nonsense. TODO
     */
    int             initNumber;

    /*
     * Data read from the 'evdev' device has to be collected into a single
     * record before we can present it to the XInput layer. Because we need
     * to do threshold comparisons, we need the current record and the
     * previous record submitted.
     *
     * This information is common insofar as we have up to three Aiptek
     * devices (cursor, stylus, eraser), and we decide on receipt of the
     * data which device to submit it to. E.g., the commonality is the
     * single tablet.
     */
    AiptekStateRec  currentValues;
    AiptekStateRec  previousValues;

    /* The physical capacity of the tablet in X, Y, and Z
     * coordinates. Comes directly from the tablet; cannot
     * be overwritten via XF86Config-4 parameters.
     */
    int             xCapacity;      /* reported from tablet: max X coord */
    int             yCapacity;      /* reported from tablet: max Y coord */
    int             zCapacity;      /* reported from tablet: max Z (pres.) */

    unsigned char   data[9];        /* data read on the device */

    int             numDevices;     /* number of tablet */
    LocalDevicePtr* deviceArray;    /* array of tablets sharing the device */

    Bool (*open)(LocalDevicePtr);   /* function to open (serial or USB) */
} AiptekCommonRec, *AiptekCommonPtr;

static InputInfoPtr xf86AiptekInit(InputDriverPtr, IDevPtr, int);
static void xf86AiptekUninit(InputDriverPtr, LocalDevicePtr, int);
static void xf86AiptekClose(LocalDevicePtr);
static LocalDevicePtr xf86AiptekAllocateStylus(void);
static LocalDevicePtr xf86AiptekAllocateCursor(void);
static LocalDevicePtr xf86AiptekAllocateEraser(void);

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))
#define ABS(x) ((x) > 0 ? (x) : -(x))

#endif
