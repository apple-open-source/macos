/*
 * Copyright 1996 by Steven Lang <tiger@tyger.org>
 *
 * AceCad tablet support ported by Arpad Gereoffy <arpi@esp-team.scene.hu>
 * originally written by Shane Watts <shane@bofh.asn.au>
 *                   and Fredrik Chabot <fhc@f6.nl>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Steven Lang not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.  Steven Lang makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * STEVEN LANG DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL STEVEN LANG BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTIONS, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/summa/xf86Summa.c,v 1.13 2003/01/12 03:55:50 tsi Exp $ */

static const char identification[] = "$Identification: 18 $";

#include "xf86Version.h"

#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(3,9,0,0,0)
#define XFREE86_V4 1
#endif

#ifdef XFREE86_V4
/* post 3.9 headers */

#ifndef XFree86LOADER
#include <unistd.h>
#include <errno.h>
#endif

#include "misc.h"
#include "xf86.h"
#define NEED_XF86_TYPES
#if !defined(DGUX)
#include "xf86_ansic.h"
#include "xisb.h"
#endif
#include "xf86_OSproc.h"
#include "xf86Xinput.h"
#include "exevents.h"		/* Needed for InitValuator/Proximity stuff */
#include "keysym.h"
#include "mipointer.h"

#ifdef XFree86LOADER
#include "xf86Module.h"
#endif

#define wait_for_fd(fd) xf86WaitForInput((fd), 1000)
#define tcflush(fd, n) xf86FlushInput((fd))
#undef read
#define read(a,b,c) xf86ReadSerial((a),(b),(c))
#undef write
#define write(a,b,c) xf86WriteSerial((a),(char*)(b),(c))
#undef close
#define close(a) xf86CloseSerial((a))
#define XCONFIG_PROBED "(==)"
#define XCONFIG_GIVEN "(**)"
#define xf86Verbose 1
#undef PRIVATE
#define PRIVATE(x) XI_PRIVATE(x)

/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning
 */

static const char *default_options[] =
{
        "BaudRate",     "9600",
        "DataBits",     "8",
        "StopBits",     "1",
        "Parity",       "Odd",
        "FlowControl",  "Xoff",
        "VTime",        "10",
        "VMin",         "1",
        NULL
};

static InputDriverPtr sumDrv;

#else /* pre 3.9 headers */

#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "XI.h"
#include "XIproto.h"

#if defined(sun) && !defined(i386)
#define POSIX_TTY
#include <errno.h>
#include <termio.h>
#include <fcntl.h>
#include <ctype.h>

#include "extio.h"
#else
#include "compiler.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSproc.h"
#ifdef XFree86LOADER
#include "xf86_ansic.h"
#endif
#include "xf86Config.h"
#include "xf86Xinput.h"
#include "atKeynames.h"
#endif

#if !defined(sun) || defined(i386)
#include "osdep.h"
#include "exevents.h"

#include "extnsionst.h"
#include "extinit.h"
#endif
#endif

/*
** Debugging macros
*/
#ifdef DBG
#undef DBG
#endif
#ifdef DEBUG
#undef DEBUG
#endif

static int      debug_level = 5;
#define DEBUG	1
#if DEBUG
#define 	DBG(lvl, f) 	{if ((lvl) <= debug_level) f;}
#else
#define 	DBG(lvl, f)
#endif

/*
** Device records (flag bits)
*/
#define ABSOLUTE_FLAG		1	/* default */
#define STYLUS_FLAG		2	/* not default */
#define COMPATIBLE_FLAG		4	/* not default */
#define H1217D_FLAG		8	/* not default */
#define Z_AXIS_FLAG		16	/* not default */
#define INITIALIZED		32

typedef struct 
{
    char	*sumDevice;	/* device file name */
    int		sumInc;		/* increment between transmits */
    int		sumButTrans;	/* button translation flags */
    int		sumOldX;	/* previous X position */
    int		sumOldY;	/* previous Y position */
    int		sumOldZ;	/* previous Z position */
    int		sumOldProximity; /* previous proximity */
    int		sumOldButtons;	/* previous buttons state */
    int		sumMaxX;	/* max X value */
    int		sumMaxY;	/* max Y value */
    int		sumXSize;	/* active area X size */
    int		sumXOffset;	/* active area X offset */
    int		sumYSize;	/* active area Y size */
    int		sumYOffset;	/* active area Y offset */
    int		sumRes;		/* resolution in lines per inch */
    int		flags;		/* various flags */
    int		sumIndex;	/* number of bytes read */
    unsigned char sumData[7];	/* data read on the device */
} SummaDeviceRec, *SummaDevicePtr;

/*
** Configuration data
*/
#define SUMMA_SECTION_NAME "SummaSketch"

#ifndef XFREE86_V4

#define PORT		1
#define DEVICENAME	2
#define THE_MODE	3
#define CURSOR		4
#define INCREMENT	5
#define BORDER		6
#define DEBUG_LEVEL     7
#define HISTORY_SIZE	8
#define ALWAYS_CORE	9
#define ACTIVE_AREA	10
#define ACTIVE_OFFSET	11
#define COMPATIBLE	12
#define RESOLUTION	13
#define HITACHI_1217D	14

#if !defined(sun) || defined(i386)
static SymTabRec SumTab[] = {
	{ENDSUBSECTION,		"endsubsection"},
	{PORT,			"port"},
	{DEVICENAME,		"devicename"},
	{THE_MODE,		"mode"},
	{CURSOR,		"cursor"},
	{INCREMENT,		"increment"},
	{BORDER,		"border"},
	{DEBUG_LEVEL,		"debuglevel"},
	{HISTORY_SIZE,		"historysize"},
	{ALWAYS_CORE,		"alwayscore"},
	{ACTIVE_AREA,		"activearea"},
	{ACTIVE_OFFSET,		"activeoffset"},
	{COMPATIBLE,		"compatible"},
	{RESOLUTION,		"resolution"},
	{HITACHI_1217D,		"hitachi_1217d"},
	{-1,			""}
};

#define RELATIVE	1
#define ABSOLUTE	2

static SymTabRec SumModeTabRec[] = {
	{RELATIVE,	"relative"},
	{ABSOLUTE,	"absolute"},
	{-1,		""}
};

#define PUCK		1
#define STYLUS		2

static SymTabRec SumPointTabRec[] = {
	{PUCK,		"puck"},
	{STYLUS,	"stylus"},
	{-1,		""}
};
  
#endif

#endif /* Pre 3.9 headers */

/*
** Contants and macro
*/
#define BUFFER_SIZE	256	/* size of reception buffer */
#define XI_NAME 	"SUMMA"	/* X device name for the stylus */

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

#define SS_TABID0	"0"	/* Tablet ID 0 */
#define SS_FIRMID	"z?"	/* Request firmware ID string */
#define SS_CONFIG	"a"	/* Send configuration (max coords) */

#define SS_ABSOLUTE	'F'	/* Absolute mode */
#define SS_RELATIVE	'E'	/* Relative mode */

#define SS_UPPER_ORIGIN	"b"	/* Origin upper left */

#define SS_PROMPT_MODE	"B"	/* Prompt mode */
#define SS_STREAM_MODE	"@"	/* Stream mode */
#define SS_INCREMENT	'I'	/* Set increment */
#define SS_BINARY_FMT	"zb"	/* Binary reporting */

#define SS_PROMPT	"P"	/* Prompt for current position */

static const char * ss_initstr = SS_TABID0 SS_UPPER_ORIGIN SS_BINARY_FMT SS_STREAM_MODE;

#define PHASING_BIT	0x80
#define PROXIMITY_BIT	0x40
#define TABID_BIT	0x20
#define XSIGN_BIT	0x10
#define YSIGN_BIT	0x08
#define BUTTON_BITS	0x07
#define COORD_BITS	0x7f

/* macro from counts/inch to counts/meter */
#define LPI2CPM(res)	(res * 1000 / 25.4)

/*
** External declarations
*/

#ifndef XFREE86_V4

#if defined(sun) && !defined(i386)
#define ENQUEUE	suneqEnqueue
#else
#define ENQUEUE	xf86eqEnqueue

extern void xf86eqEnqueue(
#if NeedFunctionPrototypes
    xEventPtr /*e*/
#endif
);
#endif

extern void miPointerDeltaCursor(
#if NeedFunctionPrototypes
    int /*dx*/,
    int /*dy*/,
    unsigned long /*time*/
#endif
);

#endif

#ifndef XFREE86_V4

#if !defined(sun) || defined(i386)
/*
** xf86SumConfig
** Reads the SummaSketch section from the XF86Config file
*/
static Bool
xf86SumConfig(LocalDevicePtr *array, int inx, int max, LexPtr val)
{
    LocalDevicePtr	dev = array[inx];
    SummaDevicePtr	priv = (SummaDevicePtr)(dev->private);
    int			token;
    int			mtoken;

    DBG(2, ErrorF("xf86SumConfig\n"));

    while ((token = xf86GetToken(SumTab)) != ENDSUBSECTION) {
	switch(token) {
	case DEVICENAME:
	    if (xf86GetToken(NULL) != STRING)
		xf86ConfigError("Option string expected");
	    else {
		dev->name = strdup(val->str);
		if (xf86Verbose)
		    ErrorF("%s SummaSketch X device name is %s\n", XCONFIG_GIVEN,
			   dev->name);
	    }
	    break;

	case PORT:
	    if (xf86GetToken(NULL) != STRING)
		xf86ConfigError("Option string expected");
	    else {
		priv->sumDevice = strdup(val->str);
		if (xf86Verbose)
		    ErrorF("%s SummaSketch port is %s\n", XCONFIG_GIVEN,
			   priv->sumDevice);
	    }
	    break;

	case THE_MODE:
	    mtoken = xf86GetToken(SumModeTabRec);
	    if ((mtoken == EOF) || (mtoken == STRING) || (mtoken == NUMBER)) 
		xf86ConfigError("Mode type token expected");
	    else {
		switch (mtoken) {
		case ABSOLUTE:
		    priv->flags |= ABSOLUTE_FLAG;
		    break;
		case RELATIVE:
		    priv->flags &= ~ABSOLUTE_FLAG;
		    break;
		default:
		    xf86ConfigError("Illegal Mode type");
		    break;
		}
	    }
	    break;

	case CURSOR:
	    mtoken = xf86GetToken(SumPointTabRec);
	    if ((mtoken == EOF) || (mtoken == STRING) || (mtoken == NUMBER)) 
		xf86ConfigError("Cursor token expected");
	    else {
		switch (mtoken) {
		case STYLUS:
		    priv->flags |= STYLUS_FLAG;
		    break;
		case PUCK:
		    priv->flags &= ~STYLUS_FLAG;
		    break;
		default:
		    xf86ConfigError("Illegal cursor type");
		    break;
		}
	    }
	    break;

	case INCREMENT:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->sumInc = val->num;
	    if (xf86Verbose)
		ErrorF("%s SummaSketch increment value is %d\n", XCONFIG_GIVEN,
		       priv->sumInc);
	    break;

	case DEBUG_LEVEL:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    debug_level = val->num;
	    if (xf86Verbose) {
#if DEBUG
		ErrorF("%s SummaSketch debug level sets to %d\n", XCONFIG_GIVEN,
		       debug_level);
#else
		ErrorF("%s SummaSketch debug level not sets to %d because"
		       " debugging is not compiled\n", XCONFIG_GIVEN,
		       debug_level);
#endif
	    }
	    break;

	case HISTORY_SIZE:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    dev->history_size = val->num;
	    if (xf86Verbose)
		ErrorF("%s SummaSketch Motion history size is %d\n", XCONFIG_GIVEN,
		       dev->history_size);      
	    break;

	 case HITACHI_1217D:
	    priv->flags |= H1217D_FLAG;
	    priv->sumInc = 0;
	    if(xf86Verbose)
		ErrorF("%s Hitach_1217D, compatible enforced.\n", XCONFIG_GIVEN);

	case COMPATIBLE:
	    priv->flags |= COMPATIBLE_FLAG;
	    if(xf86Verbose)
		ErrorF("%s SummaSketch compatible - will not query firmware ID\n", XCONFIG_GIVEN);
	    break;		    

	case ALWAYS_CORE:
	    xf86AlwaysCore(dev, TRUE);
	    if (xf86Verbose)
		ErrorF("%s SummaSketch device always stays core pointer\n",
		       XCONFIG_GIVEN);
	    break;

	case ACTIVE_AREA:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->sumXSize = val->realnum * 100;
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->sumYSize = val->realnum * 100;
	    if (xf86Verbose)
		ErrorF("%s SummaSketch active area: %d.%02dx%d.%02d"
		       " inches\n", XCONFIG_GIVEN, priv->sumXSize / 100,
		       priv->sumXSize % 100, priv->sumYSize / 100,
		       priv->sumYSize % 100);
	    break;
	    
	case ACTIVE_OFFSET:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->sumXOffset = val->realnum * 100;
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->sumYOffset = val->realnum * 100;
	    if (xf86Verbose)
		ErrorF("%s SummaSketch active offsets: %d.%02d %d.%02d"
		       " inches\n", XCONFIG_GIVEN, priv->sumXOffset / 100,
		       priv->sumXOffset % 100, priv->sumYOffset / 100,
		       priv->sumYOffset % 100);
	    break;

	case RESOLUTION:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->sumRes = val->num;
	    if (xf86Verbose)
		ErrorF("%s SummaSketch resolution set to %d\n", XCONFIG_GIVEN,
		       priv->sumRes);
	    break;

	case EOF:
	    FatalError("Unexpected EOF (missing EndSubSection)");
	    break;

	default:
	    xf86ConfigError("SummaSketch subsection keyword expected");
	    break;
	}
    }

    DBG(2, ErrorF("xf86SumConfig name=%s\n", priv->sumDevice));

    return Success;
}
#endif
#endif

/*
** xf86SumConvert
** Convert device valuator values to screen X and Y.
*/
static Bool
xf86SumConvert(LocalDevicePtr	local,
	       int		first,
	       int		num,
	       int		v0,
	       int		v1,
	       int		v2,
	       int		v3,
	       int		v4,
	       int		v5,
	       int*		x,
	       int*		y)
{
    SummaDevicePtr	priv = (SummaDevicePtr) local->private;

    if (first != 0 || num == 1)
      return FALSE;

    *x = (v0 * screenInfo.screens[0]->width) / priv->sumXSize;
    *y = (v1 * screenInfo.screens[0]->height) / priv->sumYSize;
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x > screenInfo.screens[0]->width)
	*x = screenInfo.screens[0]->width;
    if (*y > screenInfo.screens[0]->height)
	*y = screenInfo.screens[0]->height;

    DBG(6, ErrorF("Adjusted coords x=%d y=%d\n", *x, *y));

    return TRUE;
}

/*
** xf86SumReadInput
** Reads from the SummaSketch and posts any new events to the server.
*/
static void
xf86SumReadInput(LocalDevicePtr local)
{
    SummaDevicePtr	priv = (SummaDevicePtr) local->private;
    int			len, loop;
    int			is_absolute, num_ax;
    int			x, y, z, buttons, prox;
    DeviceIntPtr	device;
    unsigned char	buffer[BUFFER_SIZE];
  
    DBG(7, ErrorF("xf86SumReadInput BEGIN device=%s fd=%d\n",
       priv->sumDevice, local->fd));

    SYSCALL(len = read(local->fd, buffer, sizeof(buffer)));

    if (len <= 0) {
	Error("error reading SummaSketch device");
	return;
    }

    for(loop=0; loop<len; loop++) {

/* Format of 5 bytes data packet for SummaSketch Tablets
       Byte 1
       bit 7  Phasing bit always 1
       bit 6  Proximity bit
       bit 5  Tablet ID
       bit 4  X sign (Always 1 for absolute)
       bit 3  Y sign (Always 1 for absolute)
       bit 2-0 Button status  

       Byte 2
       bit 7  Always 0
       bits 6-0 = X6 - X0

       Byte 3 (Absolute mode only)
       bit 7  Always 0
       bits 6-0 = X13 - X7

       Byte 4
       bit 7  Always 0
       bits 6-0 = Y6 - Y0

       Byte 5 (Absolute mode only)
       bit 7  Always 0
       bits 6-0 = Y13 - Y7
   ---------------------------------
     Flair extends this with
   ---------------------------------
       Byte 6
       bit 7  Always 0
       bits 6-0 = Z8-2

       Byte 7
       bit 7-5  Always 0
       bit 4 = Z0
       bit 3-2 = Cr1-0
         Controler 00 =  4 button puck
                   01 =  3 button stylus (default)
                   10 = 16 button puck (how? only 4bits)
                   11 = reserved
       bit 1 = Button status  
       bit 0 = Z1
*/
  
	if ((priv->sumIndex == 0) && !(buffer[loop] & PHASING_BIT)) { /* magic bit is not OK */
	    DBG(6, ErrorF("xf86SumReadInput bad magic number 0x%x\n", buffer[loop]));;
	    continue;
	}

	priv->sumData[priv->sumIndex++] = buffer[loop];

	if (priv->sumIndex == ( (priv->flags & ABSOLUTE_FLAG) ?
          ((priv->flags & Z_AXIS_FLAG)?7:5) : 3)) {
/* the packet is OK */
/* reset char count for next read */
	    priv->sumIndex = 0;

	    prox = (priv->sumData[0] & PROXIMITY_BIT)? 0: 1;
	    buttons = (priv->sumData[0] & BUTTON_BITS);
	    if (priv->flags & ABSOLUTE_FLAG) {
		x = (int)priv->sumData[1] + ((int)priv->sumData[2] << 7);
		y = (int)priv->sumData[3] + ((int)priv->sumData[4] << 7);
                if(priv->flags & Z_AXIS_FLAG){
	            z = ((int)priv->sumData[5] << 2) |
			((int)priv->sumData[6] & 0x01 << 1) |
			((int)priv->sumData[6] & 0x10);
		    buttons |= ((int)priv->sumData[6] & 0x02 << 2);
                } else z = 0;
	    } else {
		x = priv->sumData[0] & XSIGN_BIT? priv->sumData[1]: -priv->sumData[1];
		y = priv->sumData[0] & YSIGN_BIT? priv->sumData[2]: -priv->sumData[2];
                z = 0;
	    }

	    /* adj x/y by user given offsets; z untouched */
	    x -= priv->sumXOffset;
	    y -= priv->sumYOffset;
	    if (x < 0) x = 0;
	    if (y < 0) y = 0;
	    if (x > priv->sumXSize) x = priv->sumXSize;
	    if (y > priv->sumYSize) y = priv->sumYSize;

	    device = local->dev;

	    DBG(6, ErrorF("prox=%s\tx=%d\ty=%d\tz=%d\tbuttons=%d\n",
		   prox ? "true" : "false", x, y, z, buttons));

	    is_absolute = (priv->flags & ABSOLUTE_FLAG);
	    num_ax = (priv->flags & Z_AXIS_FLAG)? 3 : 2;

/* coordinates are ready we can send events */
	    if (prox) {
		if (!(priv->sumOldProximity))
		    xf86PostProximityEvent(device, 1, 0, num_ax, x, y, z);

		if ((is_absolute && ((priv->sumOldX != x) || (priv->sumOldY != y) || (priv->sumOldZ != z)))
		       || (!is_absolute && (x || y))) {
		    if (is_absolute || priv->sumOldProximity) {
			xf86PostMotionEvent(device, is_absolute, 0, num_ax, x, y, z);
		    }
		}
		if (priv->sumOldButtons != buttons) {
		int	delta;
		int	button;

		    delta = buttons - priv->sumOldButtons;
		    button = (delta > 0)? delta: ((delta == 0)?
			   priv->sumOldButtons : -delta);

		    if (priv->sumOldButtons != buttons) {
			DBG(6, ErrorF("xf86SumReadInput button=%d delta=%d\n", button,
			       delta));

			xf86PostButtonEvent(device, is_absolute, button,
			       (delta > 0), 0, num_ax, x, y, z);
		    }
		}
		priv->sumOldButtons = buttons;
		priv->sumOldX = x;
		priv->sumOldY = y;
		priv->sumOldZ = z;
		priv->sumOldProximity = prox;
	    } else { /* !PROXIMITY */
/* Any changes in buttons are ignored when !proximity */
		if (priv->sumOldProximity)
		    xf86PostProximityEvent(device, 0, 0, num_ax, x, y, z);
		priv->sumOldProximity = 0;
	    }
	}
    }
    DBG(7, ErrorF("xf86SumReadInput END   device=0x%x priv=0x%x\n",
	   local->dev, priv));
}

/*
** xf86SumControlProc
** It really does do something.  Honest!
*/
static void
xf86SumControlProc(DeviceIntPtr	device, PtrCtrl *ctrl)
{
    DBG(2, ErrorF("xf86SumControlProc\n"));
}

/*
** xf86SumWriteAndRead
** Write data, and get the response.
*/
static char *
xf86SumWriteAndRead(int fd, char *data, char *buffer, int len, int cr_term)
{
    int err, numread = 0;
#ifndef XFREE86_V4
    fd_set readfds;
    struct timeval timeout;
#endif

    SYSCALL(err = write(fd, data, strlen(data)));
    if (err == -1) {
	Error("SummaSketch write");
	return NULL;
    }

#ifndef XFREE86_V4
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
#endif
    while (numread < len) {
#ifndef XFREE86_V4
	timeout.tv_sec = 0;
	timeout.tv_usec = 200000;

	SYSCALL(err = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout));
#else
	err = xf86WaitForInput(fd, 200000);
#endif
	if (err == -1) {
	    Error("SummaSketch select");
	    return NULL;
	}

	if (!err) {
	    ErrorF("Timeout while reading SummaSketch tablet. No tablet connected ???\n");
	    return NULL;
	}

	SYSCALL(err = read(fd, buffer + numread++, 1));
	if (err == -1) {
	    Error("SummaSketch read");
	    return NULL;
	}
	if (!err) {
	    --numread;
	    break;
	}
	if (cr_term && buffer[numread - 1] == '\r') {
	    buffer[numread - 1] = 0;
	    break;
	}
    }
    buffer[numread] = 0;
    return buffer;
}

/* xf86SumSetResCode
 * Set Summa MM mode resolution code letter.
*/
static void
xf86SumSetResCode (int *res, char *buffer, int index)
{
	switch (*res) {
	case 1:    buffer[index] = 'l';  break;
	case 2:    buffer[index] = 'n';  break;
	case 4:    buffer[index] = 'p';  break;
	case 100:  buffer[index] = 'd';  break;
	case 200:  buffer[index] = 'e';  break;
	case 10:		/* 10 lpmm */
	  *res = 254;
	case 254:
	  buffer[index] = 'f';  break;
	case 400:
	  buffer[index] = 'g';  break;
	case 20:		/* 20 lpmm */
	  *res = 508;
	case 508:
	  buffer[index] = 'i';  break;
	case 1000:
	  buffer[index] = 'j';  break;
	case 40:		/* 40 lpmm */
	  *res = 1016;
	case 1016:
	  buffer[index] = 'q';  break;
	case 2000:
	  buffer[index] = 's';  break;
	case 2032:
	  buffer[index] = 'u';  break;
	case 2540:		/* note: for 12x12 tablet only */
	  buffer[index] = 'v';  break;

	default:		/* default to 500 lpi */
	  *res = 500;
	  buffer[index] = 'h';  break;
	}
	buffer[index+1] = 0;
}

/*
** xf86SumOpen
** Open and initialize the tablet, as well as probe for any needed data.
*/
static Bool
xf86SumOpen(LocalDevicePtr local)
{
#ifndef XFREE86_V4
    struct termios	termios_tty;
    struct timeval	timeout;
#endif
    char		buffer[256], dbuffer[4];
    int			err, idx;
    int			res100;
    double		sratio, tratio;
    SummaDevicePtr	priv = (SummaDevicePtr)local->private;

    DBG(2, ErrorF("opening %s\n", priv->sumDevice));
#ifdef XFREE86_V4
    local->fd = xf86OpenSerial(local->options);
#else
    SYSCALL(local->fd = open(priv->sumDevice, O_RDWR|O_NDELAY, 0));
#endif
    if (local->fd == -1) {
	Error(priv->sumDevice);
	return !Success;
    }
    DBG(2, ErrorF("%s opened as fd %d\n", priv->sumDevice, local->fd));

#ifndef XFREE86_V4
#ifdef POSIX_TTY
    err = tcgetattr(local->fd, &termios_tty);
    if (err == -1) {
	Error("SummaSketch tcgetattr");
	return !Success;
    }
    termios_tty.c_iflag = IXOFF;
    termios_tty.c_cflag = B9600|CS8|CREAD|CLOCAL|HUPCL|PARENB|PARODD;
    termios_tty.c_lflag = 0;

    /* prevent tty term discipline processing */
    termios_tty.c_cc[VINTR] = 0;
    termios_tty.c_cc[VQUIT] = 0;
    termios_tty.c_cc[VERASE] = 0;
#ifdef VWERASE
    termios_tty.c_cc[VWERASE] = 0;
#endif
#ifdef VREPRINT
    termios_tty.c_cc[VREPRINT] = 0;
#endif
    termios_tty.c_cc[VKILL] = 0;
    termios_tty.c_cc[VEOF] = 0;
    termios_tty.c_cc[VEOL] = 0;
#ifdef VEOL2
    termios_tty.c_cc[VEOL2] = 0;
#endif
    termios_tty.c_cc[VSUSP] = 0;
#ifdef VDISCARD
    termios_tty.c_cc[VDISCARD] = 0;
#endif
#ifdef VLNEXT
    termios_tty.c_cc[VLNEXT] = 0; 
#endif

    termios_tty.c_cc[VMIN] = 1 ;
    termios_tty.c_cc[VTIME] = 10 ;

    err = tcsetattr(local->fd, TCSANOW, &termios_tty);
    if (err == -1) {
	Error("SummaSketch tcsetattr TCSANOW");
	return !Success;
    }
#else
#error Code for someone else to write to handle OSs without POSIX tty functions
#endif

#endif /* xf_v4 */

    DBG(2, ErrorF("initializing SummaSketch tablet\n"));

    /* send ASCII spaces in case tablet can auto-baud */
    for (idx = 0; idx < 10; idx++) buffer[idx] = ' ';
    SYSCALL(err = write(local->fd, buffer, 10));
    if (err == -1) {
	Error("SummaSketch write");
	return !Success;
    }
    buffer[0] = 0;
    SYSCALL(err = write(local->fd, buffer, 1));

/* Wait 400 mSecs, just in case.  200 ms isn't enough for the Genius EasyPen. */
#ifndef XFREE86_V4
    timeout.tv_sec = 0;
    timeout.tv_usec = 400000;
    SYSCALL(err = select(0, NULL, NULL, NULL, &timeout));
#else
    err = xf86WaitForInput(-1, 400000);
#endif
    if (err == -1) {
	Error("SummaSketch select");
	return !Success;
    }

/* Put it in prompt mode so it doens't say anything before we're ready */
    SYSCALL(err = write(local->fd, SS_PROMPT_MODE, strlen(SS_PROMPT_MODE)));
    if (err == -1) {
	Error("SummaSketch write");
	return !Success;
    }
#ifndef XFREE86_V4
/* Clear any pending input */
    tcflush(local->fd, TCIFLUSH);
#else
    xf86FlushInput(local->fd);
#endif  

    if ((priv->flags & COMPATIBLE_FLAG) == 0) {
      DBG(2, ErrorF("reading firmware ID\n"));
      if (!xf86SumWriteAndRead(local->fd, SS_FIRMID, buffer, 255, 1))
        return !Success;
      DBG(2, ErrorF("%s\n", buffer));
      if (xf86Verbose)
	ErrorF("%s %s firmware ID : %s\n", XCONFIG_PROBED, local->name, buffer);
    }

    DBG(2, ErrorF("setting up resolution\n"));
    xf86SumSetResCode (&priv->sumRes, dbuffer, 0);
    dbuffer[1] = 'a';		/* config cmd */
    dbuffer[2] = 0;

    DBG(2, ErrorF("reading max coordinates\n"));
    if (!xf86SumWriteAndRead(local->fd, dbuffer, buffer, 
                             (priv->flags&Z_AXIS_FLAG)?7:5, 0))
	return !Success;

    priv->sumMaxX = (buffer[1] & 0x7f) | (buffer[2] << 7);
    priv->sumMaxY = (buffer[3] & 0x7f) | (buffer[4] << 7);

    if (priv->flags & H1217D_FLAG) {
        /* the numbers below are from Hitachi 1217D spec sheet */
        priv->sumMaxX = (432.4 / 25.4) * priv->sumRes;
        priv->sumMaxY = (297.6 / 25.4) * priv->sumRes;
    }       

    if (xf86Verbose)
	ErrorF("%s %s: tablet size is %d.%02din. x %d.%02din., %dx%d "
	       "lines of resolution\n", XCONFIG_PROBED, local->name,
	       priv->sumMaxX / priv->sumRes,
	       (priv->sumMaxX * 100 / priv->sumRes) % 100,
	       priv->sumMaxY / priv->sumRes,
	       (priv->sumMaxY * 100 / priv->sumRes) % 100,
	       priv->sumMaxX, priv->sumMaxY);

    res100 = priv->sumRes / 100;
    if (priv->sumXOffset >= 0 && priv->sumYOffset >= 0) {
        priv->sumXOffset *= res100;
        priv->sumYOffset *= res100;
        priv->sumMaxX -= priv->sumXOffset;
        priv->sumMaxY -= priv->sumYOffset;
    }
    
    if (priv->sumXSize > 0 && priv->sumYSize > 0) {
	if ((priv->sumXSize * res100) <= priv->sumMaxX &&
	    (priv->sumYSize * res100) <= priv->sumMaxY) {
	    priv->sumXSize *= res100;
	    priv->sumYSize *= res100;
	} else {
	    ErrorF("%s %s: active area bigger than tablet, "
		   "assuming maximum\n", XCONFIG_PROBED, local->name);
	    priv->sumXSize = priv->sumMaxX;
	    priv->sumYSize = priv->sumMaxY;
	}
    } else {
	priv->sumXSize = priv->sumMaxX;
	priv->sumYSize = priv->sumMaxY;
    }

    /* map tablet area by screen aspect ratio */
    sratio = (double)screenInfo.screens[0]->height /
             (double)screenInfo.screens[0]->width;
    tratio = (double)priv->sumMaxY / (double)priv->sumMaxX;
    
    if (tratio <= 1.0) {                /* tablet horizontal > vertical */
        priv->sumXSize = (double)priv->sumYSize / sratio; 
        if (priv->sumXSize > priv->sumMaxX) priv->sumXSize = priv->sumMaxX;
    }       
    else {  
        priv->sumYSize = (double)priv->sumXSize / sratio;
        if (priv->sumYSize > priv->sumMaxY) priv->sumYSize = priv->sumMaxY;
    }              
    ErrorF("%s %s: using tablet area %d by %d, at res %d lpi\n",
        XCONFIG_PROBED, local->name, priv->sumXSize, priv->sumYSize,
	priv->sumRes);

    if (priv->sumInc > 95)
	priv->sumInc = 95;
    if (priv->sumInc < 0) {		/* increment not given by user */
/* Make a guess as to the best increment value given video mode */
	if (priv->sumXSize / screenInfo.screens[0]->width <
	       priv->sumYSize / screenInfo.screens[0]->height)
	    priv->sumInc = priv->sumXSize / screenInfo.screens[0]->width;
	else
	    priv->sumInc = priv->sumYSize / screenInfo.screens[0]->height;
	if (priv->sumInc < 1)
	    priv->sumInc = 1;
	if (xf86Verbose)
	    ErrorF("%s %s: Using increment value of %d\n", XCONFIG_PROBED,
		   local->name, priv->sumInc);
    }

/* Sets up the tablet mode to increment, stream, and such */
    for (idx = 0; ss_initstr[idx]; idx++) {
	buffer[idx] = ss_initstr[idx];
    }
    buffer[idx++] = SS_INCREMENT;
    buffer[idx++] = 32 + priv->sumInc;
    buffer[idx++] = (priv->flags & ABSOLUTE_FLAG)?
       SS_ABSOLUTE: SS_RELATIVE;
    buffer[idx] = 0;

    SYSCALL(err = write(local->fd, buffer, idx));
    if (err == -1) {
	Error("SummaSketch write");
	return !Success;
    }

    if (err <= 0) {
	SYSCALL(close(local->fd));
	return !Success;
    }

    return Success;
}

/*
** xf86SumOpenDevice
** Opens and initializes the device driver stuff or sumpthin.
*/
static int
xf86SumOpenDevice(DeviceIntPtr pSum)
{
    LocalDevicePtr	local = (LocalDevicePtr)pSum->public.devicePrivate;
    SummaDevicePtr	priv = (SummaDevicePtr)XI_PRIVATE(pSum);

    if (xf86SumOpen(local) != Success) {
	if (local->fd >= 0) {
	    SYSCALL(close(local->fd));
	}
	local->fd = -1;
    }

/* Set the real values */
    InitValuatorAxisStruct(pSum,
			   0,
			   0,			  /* min val */
			   priv->sumXSize, 	  /* max val in use */
			   LPI2CPM(priv->sumRes), /* resolution */
			   0,			  /* min_res */
			   LPI2CPM(priv->sumRes)); /* max_res */
    InitValuatorAxisStruct(pSum,
			   1,
			   0,			  /* min val */
			   priv->sumYSize, 	  /* max val in use */
			   LPI2CPM(priv->sumRes), /* resolution */
			   0,			  /* min_res */
			   LPI2CPM(priv->sumRes)); /* max_res */
    if(priv->flags&Z_AXIS_FLAG)
    InitValuatorAxisStruct(pSum,
			   2,
			   0,			/* min val */
			   512,			/* max val */
			   LPI2CPM(priv->sumRes),	/* resolution */
			   0,			/* min_res */
			   LPI2CPM(priv->sumRes));	/* max_res */

    return (local->fd != -1);
}

/* initialize start-up values */
static void
xf86SumInitPrivate (SummaDevicePtr priv)
{
#if defined (sun) && !defined(i386)
    char *dev_name;
#endif

    if (priv == NULL) return;

#if defined(sun) && !defined(i386)
    if ((dev_name = getenv("SUMMASKETCH_DEV"))) {
	priv->sumDevice = xalloc(strlen(dev_name) + 1);
	strcpy(priv->sumDevice, dev_name);
	ErrorF("xf86SumOpen port changed to '%s'\n", priv->sumDevice);
    } else {
	priv->sumDevice = "";
    }
#else
    priv->sumDevice = "";         /* device file name */
#endif
    priv->sumRes = 500;           /* default 500 lpi */
    priv->sumInc = -1;            /* event trigger delta moves thrshold */
    priv->sumOldX = -1;           /* previous X position */
    priv->sumOldY = -1;           /* previous Y position */
    priv->sumOldProximity = 0;    /* previous proximity */
    priv->sumOldButtons = 0;      /* previous buttons state */
    priv->sumMaxX = -1;           /* max X value */
    priv->sumMaxY = -1;           /* max Y value */
    priv->sumXSize = 0;	  	  /* active area X size */
    priv->sumXOffset = 0;	  /* active area X offset */
    priv->sumYSize = 0;	  	  /* active area Y size */
    priv->sumYOffset = 0;	  /* active area Y offset */
    priv->sumIndex = 0;           /* number of bytes read */
    priv->flags = ABSOLUTE_FLAG;  /* various flags -- default absolute */
}

/*
** xf86SumProc
** Handle requests to do stuff to the driver.
*/
static int
xf86SumProc(DeviceIntPtr pSum, int what)
{
    CARD8		map[25];
    int			nbaxes;
    int			nbbuttons;
    int			loop;
    LocalDevicePtr	local = (LocalDevicePtr)pSum->public.devicePrivate;
    SummaDevicePtr	priv = (SummaDevicePtr)PRIVATE(pSum);

    DBG(2, ErrorF("BEGIN xf86SumProc dev=0x%x priv=0x%x what=%d\n", pSum, priv, what));

    switch (what) {
	case DEVICE_INIT:
	    DBG(2, ErrorF("xf86SumProc pSum=0x%x fd = %d, what=INIT\n", pSum,
		local->fd));
	    if (priv->flags & INITIALIZED) break;	/* already done */

	    nbaxes = (priv->flags & Z_AXIS_FLAG)?3:2;	/* X, Y [,Z] */
	    nbbuttons = (priv->flags & STYLUS_FLAG)? ((priv->flags & Z_AXIS_FLAG)?3:2): 4;

	    for(loop=1; loop<=nbbuttons; loop++) map[loop] = loop;

	    if (InitButtonClassDeviceStruct(pSum,
					    nbbuttons,
					    map) == FALSE) {
		ErrorF("unable to allocate Button class device\n");
		return !Success;
	    }

	    if (InitFocusClassDeviceStruct(pSum) == FALSE) {
		ErrorF("unable to init Focus class device\n");
		return !Success;
	    }

	    if (InitPtrFeedbackClassDeviceStruct(pSum,
		   xf86SumControlProc) == FALSE) {
		ErrorF("unable to init ptr feedback\n");
		return !Success;
	    }

	    if (InitProximityClassDeviceStruct(pSum) == FALSE) {
		ErrorF("unable to init proximity class device\n"); 
		return !Success;
	    }

	    if (InitValuatorClassDeviceStruct(pSum,
		   nbaxes,
		   xf86GetMotionEvents,
		   local->history_size,
		   (priv->flags & ABSOLUTE_FLAG)? Absolute: Relative)
		   == FALSE) {
		ErrorF("unable to allocate Valuator class device\n"); 
		return !Success;
	    }
	    /* allocate the motion history buffer if needed */
	    xf86MotionHistoryAllocate(local);
#ifndef XFREE86_V4
	    AssignTypeAndName(pSum, local->atom, local->name);
#endif
	    /* open the device to gather informations */
	    xf86SumOpenDevice(pSum);
	    priv->flags |= INITIALIZED;
	    break;

	case DEVICE_ON:
	    DBG(2, ErrorF("xf86SumProc pSum=0x%x fd = %d, what=ON\n", pSum,
		local->fd));
	    if (pSum->public.on) break;		/* already on */

	    if ((local->fd < 0) && (!xf86SumOpenDevice(pSum))) {
		return !Success;
	    }

	    /* send prompt character to start the reports */
	    SYSCALL(write(local->fd, SS_PROMPT, strlen(SS_PROMPT)));

/* note: using xf86Add/Remove EnabledDevice() somehow causes the tablet
 * to stop responding after a VT switch, and cannot be restored w/o
 * cycling tablet power.  until we figure out what's going on, do not
 * enagae signal handlers yet.  -huver@amgraf.com mar/12/2001.
*/
#if 0
# ifdef XFREE86_V4
	    xf86AddEnabledDevice(local);
# else
	    AddEnabledDevice(local->fd);
# endif
#else
	    AddEnabledDevice(local->fd);
#endif
	    pSum->public.on = TRUE;
	    break;

	case DEVICE_OFF:
	    DBG(2, ErrorF("xf86SumProc  pSum=0x%x fd = %d, what=OFF\n", pSum,
		   local->fd));
	    if (! pSum->public.on) break;		/* already off */
	    if (local->fd >= 0)
#if 0
# ifdef XFREE86_V4
		    xf86RemoveEnabledDevice(local);
# else
	            RemoveEnabledDevice(local->fd);
# endif
#else
	            RemoveEnabledDevice(local->fd);
#endif
	    pSum->public.on = FALSE;
	    break;

	case DEVICE_CLOSE:
	    DBG(2, ErrorF("xf86SumProc  pSum=0x%x fd = %d, what=CLOSE\n", pSum,
		   local->fd));
	    if (local->fd != -1) {
	      SYSCALL(close(local->fd));
	      local->fd = -1;
	    }
	    break;

	default:
	    ErrorF("unsupported mode=%d\n", what);
	    return !Success;
	    break;
    }
    DBG(2, ErrorF("END   xf86SumProc Success what=%d dev=0x%x priv=0x%x\n",
	   what, pSum, priv));
    return Success;
}

/*
** xf86SumClose
** It...  Uh...  Closes the physical device?
*/
static void
xf86SumClose(LocalDevicePtr local)
{
    DBG(2, ErrorF("xf86SumClose local = %lx, ->fd = %d\n", local, local->fd));
    if (local->fd >= 0) {
#ifdef XFREE86_V4
	xf86CloseSerial(local->fd);
#else
	SYSCALL(close(local->fd));
#endif
    }
    local->fd = -1;
    xf86SumInitPrivate (local->private);
}

/*
** xf86SumChangeControl
** When I figure out what it does, it will do it.
*/
static int
xf86SumChangeControl(LocalDevicePtr local, xDeviceCtl* control)
{
    xDeviceResolutionCtl	*res;

    res = (xDeviceResolutionCtl *)control;
	
    if ((res->control != DEVICE_RESOLUTION) ||
	   (res->num_valuators < 1))
	return (BadMatch);

    return(Success);
}

/*
** xf86SumSwitchMode
** Switches the mode.  For now just absolute or relative, hopefully
** more on the way.
*/
static int
xf86SumSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
    LocalDevicePtr	local = (LocalDevicePtr)dev->public.devicePrivate;
    SummaDevicePtr	priv = (SummaDevicePtr)(local->private);
    char		newmode;

    DBG(3, ErrorF("xf86SumSwitchMode dev=0x%x mode=%d\n", dev, mode));

    switch(mode) {
	case Absolute:
	    priv->flags |= ABSOLUTE_FLAG;
	    newmode = SS_ABSOLUTE;
	    break;

	case Relative:
	    priv->flags &= ~ABSOLUTE_FLAG;
	    newmode = SS_RELATIVE;
	    break;

	default:
	    DBG(2, ErrorF("xf86SumSwitchMode dev=0x%x invalid mode=%d\n",
		   dev, mode));
	    return BadMatch;
    }
    SYSCALL(write(local->fd, &newmode, 1));
    return Success;
}

/*
** xf86SumAllocate
** Allocates the device structures for the SummaSketch.
*/
static LocalDevicePtr
xf86SumAllocate(void)
{
    LocalDevicePtr	local;
    SummaDevicePtr	priv;

    priv = xalloc(sizeof(SummaDeviceRec));
    if (!priv)
	return NULL;

#ifdef XFREE86_V4
    local = xf86AllocateInput(sumDrv, 0);
#else
    local = xalloc(sizeof(LocalDeviceRec));
#endif
    if (!local) {
	xfree(priv);
	return NULL;
    }

    local->name = XI_NAME;
    local->type_name = "SummaSketch Tablet";
    local->flags = 0;
#ifndef XFREE86_V4
#if !defined(sun) || defined(i386)
    local->device_config = xf86SumConfig;
#endif
#endif
    local->device_control = xf86SumProc;
    local->read_input = xf86SumReadInput;
    local->control_proc = xf86SumChangeControl;
    local->close_proc = xf86SumClose;
    local->switch_mode = xf86SumSwitchMode;
    local->conversion_proc = xf86SumConvert;
    local->fd = -1;
    local->atom = 0;
    local->dev = NULL;
    local->private = priv;
    local->private_flags = 0;
    local->history_size  = 0;
    local->old_x = -1;
    local->old_y = -1;
    xf86SumInitPrivate (priv);
    return local;
}

#ifndef XFREE86_V4
/*
** SummaSketch device association
** Device section name and allocation function.
*/
DeviceAssocRec summasketch_assoc =
{
  SUMMA_SECTION_NAME,           /* config_section_name */
  xf86SumAllocate               /* device_allocate */
};

#ifdef DYNAMIC_MODULE
/*
** init_module
** Entry point for dynamic module.
*/
int
#ifndef DLSYM_BUG
init_module(unsigned long server_version)
#else
init_xf86Summa(unsigned long server_version)
#endif
{
    xf86AddDeviceAssoc(&summasketch_assoc);

    if (server_version != XF86_VERSION_CURRENT) {
	ErrorF("Warning: SummaKetch module compiled for version%s\n",
	       XF86_VERSION);
	return 0;
    } else {
	return 1;
    }
}
#endif /* dynamic_module */

#else  /* below for xfree86_v4 */

/*
 * xf86SumUninit --
 *
 * called when the driver is unloaded.
 */
static void
xf86SumUninit(InputDriverPtr	drv,
	      LocalDevicePtr	local,
	      int flags)
{
    SummaDevicePtr	priv = (SummaDevicePtr) local->private;
    
    DBG(2, ErrorF("xf86SumUninit\n"));
    
    xf86SumProc(local->dev, DEVICE_OFF);
    
    xfree (priv);
    xf86DeleteInput(local, 0);    
}

/*
 * xf86SumInit --
 *
 * called when the module subsection is found in XF86Config
 */
static InputInfoPtr
xf86SumInit(InputDriverPtr	drv,
	    IDevPtr		dev,
	    int			flags)
{
    LocalDevicePtr	local = NULL;
    SummaDevicePtr	priv = NULL;
    char		*s;

    sumDrv = drv;

    DBG(2, ErrorF("xf86SumInit allocating...\n"));

    local = xf86SumAllocate();
    if (!local)
	return NULL;

    local->conf_idev = dev;

    DBG(2, ErrorF("xf86SumInit CollectInputOptions... "));
    xf86CollectInputOptions(local, default_options, NULL);   
    DBG(2, ErrorF("done.\n"));
    xf86OptionListReport( local->options );

    priv = (SummaDevicePtr) local->private;

    local->name = dev->identifier;
    
    /* Serial Device name is mandatory */
    priv->sumDevice = xf86FindOptionValue(local->options, "Device");

    if (!priv->sumDevice) {
	xf86Msg (X_ERROR, "%s: No Device specified.\n", dev->identifier);
	goto SetupProc_fail;
    }

    /* Process the common options. */
    xf86ProcessCommonOptions(local, local->options);

    /* Optional configuration */

    xf86Msg(X_CONFIG, "%s: serial device is %s\n", dev->identifier,
	    priv->sumDevice);

    debug_level = xf86SetIntOption(local->options, "DebugLevel", 0);
    if (debug_level > 0) {
	xf86Msg(X_CONFIG, "Summa: debug level set to %d\n", debug_level);
    }

    priv->sumInc = xf86SetIntOption(local->options, "Increment", -1);
    if (priv->sumInc >= 0) {
	xf86Msg(X_CONFIG, "%s: increment set to %d\n", local->name, priv->sumInc);
    }

    s = xf86FindOptionValue(local->options, "Resolution");
    if (s) {
      priv->sumRes=atoi(s);
      xf86Msg(X_CONFIG, "%s: resolution given %d\n", local->name, priv->sumRes);
    }

    s = xf86FindOptionValue(local->options, "Mode");

    if (s && (xf86NameCmp(s, "absolute") == 0)) {
	priv->flags = priv->flags | ABSOLUTE_FLAG;
    }
    else if (s && (xf86NameCmp(s, "relative") == 0)) {
	priv->flags = priv->flags & ~ABSOLUTE_FLAG;
    }
    else if (s) {
	xf86Msg(X_ERROR, "%s: invalid Mode (should be absolute or relative). "
		"Using default.\n", dev->identifier);
    }
    xf86Msg(X_CONFIG, "%s: set for %s mode\n", local->name,
	    (priv->flags & ABSOLUTE_FLAG) ? "absolute" : "relative");	    

    s = xf86FindOption(local->options, "Compatible");
    if (s) {
      priv->flags|=COMPATIBLE_FLAG;
    }

    if (xf86FindOption(local->options, "Hitachi_1217D") != NULL) {
       priv->flags |= H1217D_FLAG | COMPATIBLE_FLAG;
       priv->sumInc = 0;	/* this must be zero for Hitachi 1217D */
       xf86Msg(X_CONFIG, "%s: Hitachi 1217D, Compatible mode and Increment 0 implied\n",
		local->name);
    }
    if (priv->flags & COMPATIBLE_FLAG) {
       xf86Msg(X_CONFIG, "%s: will not query firmware ID.\n", local->name);
    }

    s = xf86FindOptionValue(local->options, "ZAxis");
    if(s){
       priv->flags|=Z_AXIS_FLAG;
       xf86Msg(X_CONFIG, "Tablet has Z axis (AceCad Flair or compatible)\n");
    }

    s = xf86FindOptionValue(local->options, "Cursor");

    if (s && (xf86NameCmp(s, "stylus") == 0)) {
	priv->flags = priv->flags | STYLUS_FLAG;
    }
    else if (s && (xf86NameCmp(s, "puck") == 0)) {
	priv->flags = priv->flags & ~STYLUS_FLAG;
    }
    else if (s) {
	xf86Msg(X_ERROR, "%s: invalid Cursor (should be stylus or puck). "
		"Using default.\n", dev->identifier);
    }
    xf86Msg(X_CONFIG, "%s: cursor mode is %s\n", local->name,
	    (priv->flags & STYLUS_FLAG) ? "cursor" : "puck");	    

    priv->sumXSize = xf86SetRealOption(local->options, "XSize", 0.0) * 100;
    if (priv->sumXSize != 0) {
	    xf86Msg(X_CONFIG, "%s: XSize = %d.%02din.\n", 
		    dev->identifier, priv->sumXSize/100, priv->sumXSize%100);
    }

    priv->sumYSize = xf86SetRealOption(local->options, "YSize", 0.0) * 100;
    if (priv->sumYSize != 0) {
	    xf86Msg(X_CONFIG, "%s: YSize = %d.%02din.\n", 
		    dev->identifier, priv->sumYSize/100, priv->sumYSize%100);
    }

    priv->sumXOffset = xf86SetRealOption(local->options, "XOffset", 0.0) * 100;
    if (priv->sumXOffset != 0) {
	    xf86Msg(X_CONFIG, "%s: XOffset = %d.%02din.\n",
		    dev->identifier, priv->sumXOffset / 100,
		    priv->sumXOffset % 100);
    }

    priv->sumYOffset = xf86SetRealOption(local->options, "YOffset", 0.0) * 100;
    if (priv->sumYOffset != 0) {
	    xf86Msg(X_CONFIG, "%s: YOffset = %d.%02din.\n", 
		    dev->identifier, priv->sumYOffset / 100,
		    priv->sumYOffset % 100);
    }

    /* mark the device configured */
    local->flags |= XI86_POINTER_CAPABLE | XI86_CONFIGURED;

    /* return the LocalDevice */
    return local;

  SetupProc_fail:
    if (priv)
	xfree(priv);
    return local;
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec SUMMA = {
    1,				/* driver version */
    "summa",			/* driver name */
    NULL,			/* identify */
    xf86SumInit,		/* pre-init */
    xf86SumUninit,		/* un-init */
    NULL,			/* module */
    0				/* ref count */
};


/*
 ***************************************************************************
 *
 * Dynamic loading functions
 *
 ***************************************************************************
 */
#ifdef XFree86LOADER
/*
 * xf86SumUnplug --
 *
 * called when the module subsection is found in XF86Config
 */
static void
xf86SumUnplug(pointer	p)
{
}

/*
 * xf86SumPlug --
 *
 * called when the module subsection is found in XF86Config
 */
static pointer
xf86SumPlug(pointer	module,
	    pointer	options,
	    int		*errmaj,
	    int		*errmin)
{
    xf86AddInputDriver(&SUMMA, module, 0);

    return module;
}

static XF86ModuleVersionInfo xf86SumVersionRec =
{
    "summa",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    1, 0, 0,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}		/* signature, to be patched into the file by */
				/* a tool */
};

XF86ModuleData summaModuleData = {&xf86SumVersionRec,
				  xf86SumPlug,
				  xf86SumUnplug};

#endif /* XFree86LOADER */
#endif /* XFREE86_V4 */


/* end of xf86Summa.c */

