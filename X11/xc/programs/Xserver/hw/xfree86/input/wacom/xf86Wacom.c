/* $XConsortium: xf86Wacom.c /main/20 1996/10/27 11:05:20 kaleb $ */
/*
 * Copyright 1995-2001 by Frederic Lepied, France. <Lepied@XFree86.org>
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/wacom/xf86Wacom.c,v 1.45 2003/12/31 01:18:45 tsi Exp $ */

/*
 * This driver is only able to handle the Wacom IV and Wacom V protocols.
 *
 * Wacom V protocol work done by Raph Levien <raph@gtk.org> and
 * Frédéric Lepied <lepied@xfree86.org>.
 *
 * Many thanks to Dave Fleck from Wacom for the help provided to
 * build this driver.
 *
 * Modified for Linux USB by MATSUMURA Namihiko,
 * Daniel Egger, Germany. <egger@suse.de>,
 * Frederic Lepied <lepied@xfree86.org>,
 * Brion Vibber <brion@pobox.com>,
 * Aaron Optimizer Digulla <digulla@hepe.com>,
 * Jonathan Layes <jonathan@layes.com>,
 * Ping Cheng <pingc@wacom.com>,
 * John Joganic <john@joganic.com>.
 *
 */

static const char identification[] = "$Identification: 42 $";

#include <xf86Version.h>

#ifdef LINUX_INPUT
#include <asm/types.h>
#include <linux/input.h>

/* 2.4.6 module support */
#ifndef EV_MSC
#define EV_MSC 0x04
#endif

#ifndef MSC_SERIAL
#define MSC_SERIAL 0x00
#endif

/* max number of input events to read in one read call */
#define MAX_EVENTS 50

/* keithp - a hack to avoid redefinitions of these in xf86str.h */
#ifdef BUS_PCI
#undef BUS_PCI
#endif
#ifdef BUS_ISA
#undef BUS_ISA
#endif
#endif

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

#define xf86WcmWaitForTablet(fd) xf86WaitForInput((fd), 1000000)
#define xf86WcmFlushTablet(fd) xf86FlushInput((fd))
#define xf86WcmOpenTablet(local) xf86OpenSerial((local)->options)
#define xf86WcmSetSerialSpeed(fd,rate) xf86SetSerialSpeed((fd),(rate))
#define xf86WcmRead(a,b,c) xf86ReadSerial((a),(b),(c))
#define xf86WcmWrite(a,b,c) xf86WriteSerial((a),(char*)(b),(c))
#define xf86WcmClose(a) xf86CloseSerial((a))
#define XCONFIG_PROBED "(==)"
#define XCONFIG_GIVEN "(**)"
#define xf86Verbose 1
#undef PRIVATE
#define PRIVATE(x) XI_PRIVATE(x)

/******************************************************************************
 * Forward Declarations
 *****************************************************************************/

typedef struct _WacomModule WacomModule;
typedef struct _WacomModule4 WacomModule4;
typedef struct _WacomModule3 WacomModule3;
typedef struct _WacomModel WacomModel, *WacomModelPtr;
typedef struct _WacomDeviceRec WacomDeviceRec, *WacomDevicePtr;
typedef struct _WacomDeviceState WacomDeviceState, *WacomDeviceStatePtr;
typedef struct _WacomChannel  WacomChannel, *WacomChannelPtr;
typedef struct _WacomCommonRec WacomCommonRec, *WacomCommonPtr;
typedef struct _WacomFilterState WacomFilterState, *WacomFilterStatePtr;
typedef struct _WacomDeviceClass WacomDeviceClass, *WacomDeviceClassPtr;

/*****************************************************************************
 * General Inlined functions and Prototypes
 ****************************************************************************/

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))
#define RESET_RELATIVE(ds) do { (ds).relwheel = 0; } while (0)

static int xf86WcmWait(int t);
static int xf86WcmReady(int fd);

static LocalDevicePtr xf86WcmAllocate(char* name, int flag);
static LocalDevicePtr xf86WcmAllocateStylus(void);
static LocalDevicePtr xf86WcmAllocateCursor(void);
static LocalDevicePtr xf86WcmAllocateEraser(void);

static Bool xf86WcmOpen(LocalDevicePtr local);
static int xf86WcmDevOpen(DeviceIntPtr pWcm);
static int xf86WcmInitTablet(WacomCommonPtr common, WacomModelPtr model,
	int fd, const char* id, float version);

static void xf86WcmDevReadInput(LocalDevicePtr local);
static void xf86WcmReadPacket(LocalDevicePtr local);

static void xf86WcmEvent(WacomCommonPtr common,
	unsigned int channel, const WacomDeviceState* ds);

/* Serial Support */
static int xf86WcmSerialValidate(WacomCommonPtr common, const unsigned char* data);
static Bool serialDetect(LocalDevicePtr pDev);
static Bool serialInit(LocalDevicePtr pDev);

static int serialInitTablet(WacomCommonPtr common, int fd);
static void serialInitIntuos(WacomCommonPtr common, int fd,
        const char* id, float version);
static void serialInitIntuos2(WacomCommonPtr common, int fd,
        const char* id, float version);
static void serialInitCintiq(WacomCommonPtr common, int fd,
        const char* id, float version);
static void serialInitPenPartner(WacomCommonPtr common, int fd,
        const char* id, float version);
static void serialInitGraphire(WacomCommonPtr common, int fd,
        const char* id, float version);
static void serialInitProtocol4(WacomCommonPtr common, int fd,
        const char* id, float version);
static void serialGetResolution(WacomCommonPtr common, int fd);
static int serialGetRanges(WacomCommonPtr common, int fd);
static int serialResetIntuos(WacomCommonPtr common, int fd);
static int serialResetCintiq(WacomCommonPtr common, int fd);
static int serialResetPenPartner(WacomCommonPtr common, int fd);
static int serialResetProtocol4(WacomCommonPtr common, int fd);
static int serialEnableTiltProtocol4(WacomCommonPtr common, int fd);
static int serialEnableSuppressProtocol4(WacomCommonPtr common, int fd);
static int serialSetLinkSpeedIntuos(WacomCommonPtr common, int fd);
static int serialSetLinkSpeedProtocol5(WacomCommonPtr common, int fd);
static int serialStartTablet(WacomCommonPtr common, int fd);
static int serialParseCintiq(WacomCommonPtr common,
        const unsigned char* data);
static int serialParseGraphire(WacomCommonPtr common,
        const unsigned char* data);
static int serialParseProtocol4(WacomCommonPtr common,
        const unsigned char* data);
static int serialParseProtocol5(WacomCommonPtr common,
        const unsigned char* data);
static void serialParseP4Common(WacomCommonPtr common,
        const unsigned char* data, WacomDeviceState* last,
        WacomDeviceState* ds);

#ifdef LINUX_INPUT
static Bool usbDetect(LocalDevicePtr local);
static Bool usbInit(LocalDevicePtr local);
static void usbInitProtocol5(WacomCommonPtr common, int fd, const char* id,
        float version);
static void usbInitProtocol4(WacomCommonPtr common, int fd, const char* id,
        float version);
static int usbGetRanges(WacomCommonPtr common, int fd);
static int usbParse(WacomCommonPtr common, const unsigned char* data);
static void usbParseEvent(WacomCommonPtr common,
        const struct input_event* event);
static void usbParseChannel(WacomCommonPtr common, int channel, int serial);
#endif

static Bool isdv4Detect(LocalDevicePtr);
static Bool isdv4Init(LocalDevicePtr);
static void isdv4InitISDV4(WacomCommonPtr common, int fd, const char* id,
        float version);
static int isdv4Parse(WacomCommonPtr common, const unsigned char* data);

static int xf86WcmFilterCoord(WacomCommonPtr common, WacomChannelPtr pChannel,
        WacomDeviceStatePtr ds);
static int xf86WcmFilterIntuos(WacomCommonPtr common, WacomChannelPtr pChannel,
        WacomDeviceStatePtr ds);

static void xf86WcmSendEvents(LocalDevicePtr local, const WacomDeviceState* ds);
static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int	num,
	       int v0, int v1, int v2, int v3, int v4, int v5, int *x, int *y);
static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int *valuators);
static int xf86WcmDevProc(DeviceIntPtr pWcm, int what);
static void xf86WcmDevControlProc(DeviceIntPtr device, PtrCtrl *ctrl);
static void xf86WcmDevClose(LocalDevicePtr local);
static int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl* control);
static int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);

/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning
 */
static const char *default_options[] =
{
	"BaudRate", "9600",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "None",
	"Vmin", "1",
	"Vtime", "10",
	"FlowControl", "Xoff",
	NULL
};

#if defined(__QNX__) || defined(__QNXNTO__)
#define POSIX_TTY
#endif

/******************************************************************************
 * debugging macro
 *****************************************************************************/
#ifdef DBG
#undef DBG
#endif
#ifdef DEBUG
#undef DEBUG
#endif

static int      debug_level = 0;
#define DEBUG 1
#if DEBUG
#define DBG(lvl, f) {if ((lvl) <= debug_level) f;}
#else
#define DBG(lvl, f)
#endif

#define ABS(x) ((x) > 0 ? (x) : -(x))
#define mils(res) (res * 100 / 2.54) /* resolution */

/******************************************************************************
 * WacomDeviceRec flags
 *****************************************************************************/
#define DEVICE_ID(flags) ((flags) & 0x07)

#define STYLUS_ID		1
#define CURSOR_ID		2
#define ERASER_ID		4
#define ABSOLUTE_FLAG		8
#define	KEEP_SHAPE_FLAG		16
#define BAUD_19200_FLAG		32
#define BETA_FLAG		64
#define BUTTONS_ONLY_FLAG	128

#define IsCursor(priv) (DEVICE_ID((priv)->flags) == CURSOR_ID)
#define IsStylus(priv) (DEVICE_ID((priv)->flags) == STYLUS_ID)
#define IsEraser(priv) (DEVICE_ID((priv)->flags) == ERASER_ID)

#define MAX_SAMPLES 4

#define PEN(ds)         (((ds->device_id) & 0x07ff) == 0x0022 \
			|| ((ds->device_id) & 0x07ff) == 0x0042 \
			|| ((ds->device_id) & 0x07ff) == 0x0052)
#define STROKING_PEN(ds) (((ds->device_id) & 0x07ff) == 0x0032)
#define AIRBRUSH(ds)    (((ds->device_id) & 0x07ff) == 0x0112)
#define MOUSE_4D(ds)    (((ds->device_id) & 0x07ff) == 0x0094)
#define MOUSE_2D(ds)    (((ds->device_id) & 0x07ff) == 0x0007)
#define LENS_CURSOR(ds) (((ds->device_id) & 0x07ff) == 0x0096)
#define INKING_PEN(ds)  (((ds->device_id) & 0x07ff) == 0x0012)
#define STYLUS_TOOL(ds) (PEN(ds) || STROKING_PEN(ds) || INKING_PEN(ds) || \
                        AIRBRUSH(ds))
#define CURSOR_TOOL(ds) (MOUSE_4D(ds) || LENS_CURSOR(ds) || MOUSE_2D(ds))

typedef int (*FILTERFUNC)(WacomDevicePtr pDev, WacomDeviceStatePtr pState);

/* FILTERFUNC return values:
 *   -1 - data should be discarded
 *    0 - data is valid */

#define FILTER_PRESSURE_RES 2048        /* maximum points in pressure curve */

typedef enum { TV_NONE = 0, TV_ABOVE_BELOW = 1, TV_LEFT_RIGHT = 2 } tvMode;

/******************************************************************************
 * configuration stuff
 *****************************************************************************/
#define CURSOR_SECTION_NAME "wacomcursor"
#define STYLUS_SECTION_NAME "wacomstylus"
#define ERASER_SECTION_NAME "wacomeraser"


/******************************************************************************
 * constant and macros declarations
 *****************************************************************************/
#define DEFAULT_SPEED 1.0	/* default relative cursor speed */
#define MAX_ACCEL 7             /* number of acceleration levels */
#define DEFAULT_SUPPRESS 2	/* default suppress */
#define MAX_SUPPRESS 6  	/* max value of suppress */
#define BUFFER_SIZE 256		/* size of reception buffer */
#define XI_STYLUS "STYLUS"	/* X device name for the stylus */
#define XI_CURSOR "CURSOR"	/* X device name for the cursor */
#define XI_ERASER "ERASER"	/* X device name for the eraser */
#define MAX_VALUE 100           /* number of positions */
#define MAXTRY 3                /* max number of try to receive magic number */
#define MAX_COORD_RES 1270.0	/* Resolution of the returned MaxX and MaxY */

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

#define WC_RESET	"\r#" /* reset to wacom IV command set or wacom V reset */
#define WC_RESET_BAUD	"\r$" /* reset baud rate to default (wacom V) or switch to wacom IIs (wacom IV) */
#define WC_CONFIG	"~R\r"	/* request a configuration string */
#define WC_COORD	"~C\r"	/* request max coordinates */
#define WC_MODEL	"~#\r"	/* request model and ROM version */

#define WC_MULTI	"MU1\r"	/* multi mode input */
#define WC_UPPER_ORIGIN	"OC1\r"	/* origin in upper left */
#define WC_SUPPRESS	"SU"	/* suppress mode */
#define WC_ALL_MACRO	"~M0\r"	/* enable all macro buttons */
#define WC_NO_MACRO1	"~M1\r"	/* disable macro buttons of group 1 */
#define WC_RATE 	"IT0\r"	/* max transmit rate (unit of 5 ms) */
#define WC_TILT_MODE	"FM1\r"	/* enable extra protocol for tilt management */
#define WC_NO_INCREMENT	"IN0\r"	/* do not enable increment mode */
#define WC_STREAM_MODE	"SR\r"	/* enable continuous mode */
#define WC_PRESSURE_MODE "PH1\r" /* enable pressure mode */
#define WC_ZFILTER	"ZF1\r" /* stop sending coordinates */
#define WC_STOP		"\nSP\r" /* stop sending coordinates */
#define WC_START	"ST\r"	/* start sending coordinates */
#define WC_NEW_RESOLUTION "NR"	/* change the resolution */

/******************************************************************************
 * WacomCommonRec flags
 *****************************************************************************/
#define TILT_REQUEST_FLAG	1
#define TILT_ENABLED_FLAG	2
#define RAW_FILTERING_FLAG	4

#define DEVICE_ISDV4 0x000C

#define ROTATE_NONE 0
#define ROTATE_CW 1
#define ROTATE_CCW 2

#define MAX_CHANNELS 2
#define MAX_USB_EVENTS 32

#define HANDLE_TILT(comm) ((comm)->wcmFlags & TILT_ENABLED_FLAG)
#define RAW_FILTERING(comm) ((comm)->wcmFlags & RAW_FILTERING_FLAG)

struct _WacomFilterState
{
    int		npoints;
    int		x[3];
    int		y[3];
    int		tiltx[3];
    int		tilty[3];
    int		statex;
    int		statey;
};

struct _WacomDeviceState
{
    int			device_id;
    int			device_type;
    unsigned int	serial_num;
    int			x;
    int			y;
    int			buttons;
    int			pressure;
    int			tiltx;
    int			tilty;
    int			rotation;
    int			abswheel;
    int			relwheel;
    int			distance;
    int			throttle;
    int			discard_first;
    int			proximity;
    int			sample;     /* wraps every 24 days */
};

struct _WacomChannel
{
    /* data stored in this structure is raw data from the tablet, prior
     * to transformation and user-defined filtering. Suppressed values
     * will not be included here, and hardware filtering may occur between
     * the work stage and the valid state. */

    WacomDeviceState work;                         /* next state */

    /* the following struct contains the current known state of the
     * device channel, as well as the previous MAX_SAMPLES states
     * for use in detecting hardware defects, jitter, trends, etc. */

    /* the following union contains the current known state of the
     * device channel, as well as the previous MAX_SAMPLES states
     * for use in detecting hardware defects, jitter, trends, etc. */
    union
    {
	WacomDeviceState state;                /* current state */
	WacomDeviceState states[MAX_SAMPLES];  /* states 0..MAX */
    } valid;

    int nSamples;
    LocalDevicePtr pDev;    /* device associated with this channel */
    WacomFilterState rawFilter;
};

struct _WacomDeviceRec
{
    /* configuration fields */
    unsigned int	flags;		/* various flags (device type, absolute, touch...) */
    int			topX;		/* X top */
    int			topY;		/* Y top */
    int			bottomX;	/* X bottom */
    int			bottomY;	/* Y bottom */
    double		factorX;	/* X factor */
    double		factorY;	/* Y factor */
    unsigned int	serial;	        /* device serial number */
    int			screen_no;	/* associated screen */
    int			button[16];	/* buttons */
    
    WacomCommonPtr	common;		/* common info pointer */
    
    /* state fields */
    int			oldX;		/* previous X position */
    int			oldY;		/* previous Y position */
    int			oldZ;		/* previous pressure */
    int			oldTiltX;	/* previous tilt in x direction */
    int			oldTiltY;	/* previous tilt in y direction */    
    int			oldWheel;	/* previous wheel value */    
    int			oldRot;		/* previous rotation value */
    int			oldThrottle;	/* previous throttle value */
    int			oldButtons;	/* previous buttons state */
    int			oldProximity;	/* previous proximity */
    double		speed;          /* relative mode speed */
    int			accel;		/* relative mode acceleration */
    int			numScreen;	/* number of configured screens */
    int			currentScreen;	/* current screen in display */
    double		dscaleX;	/* scale in X for dual screens */
    double		dscaleY;	/* scale in Y for dual screens */
    int			doffsetX;	/* offset in X for dual screens */
    int			doffsetY;	/* offset in Y for dual screens */
    tvMode		twinview;	/* using twinview mode of gfx card */
    int			tvResolution[4];/* twinview screens' resultion */
    int			throttleStart;	/* time in ticks for last wheel movement */
    int			throttleLimit;	/* time in ticks for next wheel movement */
    int			throttleValue;	/* current throttle value */
    int			*pPressCurve;	/* pressure curve */
    int			nPressCtrl[4];	/* control points for curve */
};

struct _WacomCommonRec 
{
    char		*wcmDevice;	/* device file name */
    int			wcmSuppress;	/* transmit position if increment is superior */
    unsigned char	wcmFlags;	/* various flags (handle tilt) */
    int			wcmMaxX;	/* max X value */
    int			wcmMaxY;	/* max Y value */
    int			wcmMaxZ;	/* max Z value */
    int			wcmResolX;	/* X resolution in points/inch */
    int			wcmResolY;	/* Y resolution in points/inch */
    int			wcmUserResolX;	/* user-defined X resolution */
    int			wcmUserResolY;	/* user-defined Y resolution */
    int			wcmUserResolZ;	/* user-defined Z resolution,
					 * value equal to 100% pressure */
    LocalDevicePtr	*wcmDevices;	/* array of all devices sharing the same port */
    int			wcmNumDevices;	/* number of devices */
    int			wcmPktLength;	/* length of a packet */
    int			wcmProtocolLevel;/* 4 for Wacom IV, 5 for Wacom V */
    float		wcmVersion; 	/* ROM version */
    int			wcmForceDevice;	/* force device type (used by ISD V4) */
    int			wcmRotate;	/* rotate screen (for TabletPC) */
    int			wcmThreshold;	/* Threshold for button pressure */
    int			wcmChannelCnt;	/* number of channels available */
    WacomChannel	wcmChannel[MAX_CHANNELS]; /* channel device state */
    int			wcmInitialized;	/* device is initialized */
    unsigned int	wcmLinkSpeed;	/* serial link speed */

    WacomDeviceClassPtr	wcmDevCls;	/* device class functions */
    WacomModelPtr	wcmModel; 	/* model-specific functions */
    int			wcmGimp;	/* support Gimp on Xinerama Enabled multi-monitor desktop */

    int			bufpos;		/* position with buffer */
    unsigned char	buffer[BUFFER_SIZE]; /* data read from device */

#ifdef LINUX_INPUT
    int			wcmEventCnt;
    struct input_event	wcmEvents[MAX_USB_EVENTS];  /* events for current change */
#endif
};

struct _WacomModule4
{
    InputDriverPtr wcmDrv;
};

struct _WacomModule
{
    int debugLevel;
    KeySym* keymap;
    const char* identification;
    WacomModule4 v4;
    int (*DevOpen)(DeviceIntPtr pWcm);
    void (*DevReadInput)(LocalDevicePtr local);
    void (*DevControlProc)(DeviceIntPtr device, PtrCtrl* ctrl);
    void (*DevClose)(LocalDevicePtr local);
    int (*DevProc)(DeviceIntPtr pWcm, int what);
    int (*DevChangeControl)(LocalDevicePtr local, xDeviceCtl* control);
    int (*DevSwitchMode)(ClientPtr client, DeviceIntPtr dev, int mode);
    Bool (*DevConvert)(LocalDevicePtr local, int first, int num,
                int v0, int v1, int v2, int v3, int v4, int v5,
                int* x, int* y);
    Bool (*DevReverseConvert)(LocalDevicePtr local, int x, int y,
                int* valuators);
};

/******************************************************************************
 * WacomModel - model-specific device capabilities
 *****************************************************************************/

struct _WacomModel
{
    const char* name;

    void (*Initialize)(WacomCommonPtr common, int fd, const char* id, float version);
    void (*GetResolution)(WacomCommonPtr common, int fd);
    int (*GetRanges)(WacomCommonPtr common, int fd);
    int (*Reset)(WacomCommonPtr common, int fd);
    int (*EnableTilt)(WacomCommonPtr common, int fd);
    int (*EnableSuppress)(WacomCommonPtr common, int fd);
    int (*SetLinkSpeed)(WacomCommonPtr common, int fd);
    int (*Start)(WacomCommonPtr common, int fd);
    int (*Parse)(WacomCommonPtr common, const unsigned char* data);
    int (*FilterRaw)(WacomCommonPtr common, WacomChannelPtr pChannel,
                WacomDeviceStatePtr ds);
};

static const char * setup_string = WC_MULTI WC_UPPER_ORIGIN
 WC_ALL_MACRO WC_NO_MACRO1 WC_RATE WC_NO_INCREMENT WC_STREAM_MODE WC_ZFILTER;

static const char * pl_setup_string = WC_UPPER_ORIGIN WC_RATE WC_STREAM_MODE;

static const char * penpartner_setup_string = WC_PRESSURE_MODE WC_START;

#define WC_V_SINGLE	"MT0\r"
#define WC_V_MULTI	"MT1\r"
#define WC_V_HEIGHT	"HT1\r"
#define WC_V_ID		"ID1\r"
#define WC_V_19200	"BA19\r"
#define WC_V_38400	"BA38\r"
/*  #define WC_V_9600	"BA96\r" */
#define WC_V_9600	"$\r"

#define WC_RESET_19200	"\r$"	/* reset to 9600 baud */
#define WC_RESET_19200_IV "\r#"

static const char * intuos_setup_string = WC_V_MULTI WC_V_ID WC_RATE WC_START;

#define COMMAND_SET_MASK	0xc0
#define BAUD_RATE_MASK		0x0a
#define PARITY_MASK		0x30
#define DATA_LENGTH_MASK	0x40
#define STOP_BIT_MASK		0x80

#define HEADER_BIT	0x80
#define ZAXIS_SIGN_BIT	0x40
#define ZAXIS_BIT    	0x04
#define ZAXIS_BITS    	0x3f
#define POINTER_BIT     0x20
#define PROXIMITY_BIT   0x40
#define BUTTON_FLAG	0x08
#define BUTTONS_BITS	0x78
#define TILT_SIGN_BIT	0x40
#define TILT_BITS	0x3f

/* defines to discriminate second side button and the eraser */
#define ERASER_PROX	4
#define OTHER_PROX	1

/******************************************************************************
 * Function/Macro keys variables
 *****************************************************************************/
static KeySym wacom_map[] = 
{
    NoSymbol,	/* 0x00 */
    NoSymbol,	/* 0x01 */
    NoSymbol,	/* 0x02 */
    NoSymbol,	/* 0x03 */
    NoSymbol,	/* 0x04 */
    NoSymbol,	/* 0x05 */
    NoSymbol,	/* 0x06 */
    NoSymbol,	/* 0x07 */
    XK_F1,	/* 0x08 */
    XK_F2,	/* 0x09 */
    XK_F3,	/* 0x0a */
    XK_F4,	/* 0x0b */
    XK_F5,	/* 0x0c */
    XK_F6,	/* 0x0d */
    XK_F7,	/* 0x0e */
    XK_F8,	/* 0x0f */
    XK_F9,	/* 0x10 */
    XK_F10,	/* 0x11 */
    XK_F11,	/* 0x12 */
    XK_F12,	/* 0x13 */
    XK_F13,	/* 0x14 */
    XK_F14,	/* 0x15 */
    XK_F15,	/* 0x16 */
    XK_F16,	/* 0x17 */
    XK_F17,	/* 0x18 */
    XK_F18,	/* 0x19 */
    XK_F19,	/* 0x1a */
    XK_F20,	/* 0x1b */
    XK_F21,	/* 0x1c */
    XK_F22,	/* 0x1d */
    XK_F23,	/* 0x1e */
    XK_F24,	/* 0x1f */
    XK_F25,	/* 0x20 */
    XK_F26,	/* 0x21 */
    XK_F27,	/* 0x22 */
    XK_F28,	/* 0x23 */
    XK_F29,	/* 0x24 */
    XK_F30,	/* 0x25 */
    XK_F31,	/* 0x26 */
    XK_F32	/* 0x27 */
};

/* minKeyCode = 8 because this is the min legal key code */
static KeySymsRec wacom_keysyms = {
  /* map	minKeyCode	maxKC	width */
  wacom_map,	8,		0x27,	1
};

#define XWACOM_PARAM_TOPX       1
#define XWACOM_PARAM_TOPY       2
#define XWACOM_PARAM_BOTTOMX    3
#define XWACOM_PARAM_BOTTOMY    4
#define XWACOM_PARAM_BUTTON1    5
#define XWACOM_PARAM_BUTTON2    6
#define XWACOM_PARAM_BUTTON3    7
#define XWACOM_PARAM_BUTTON4    8
#define XWACOM_PARAM_BUTTON5    9
#define XWACOM_PARAM_DEBUGLEVEL 10
#define XWACOM_PARAM_PRESSCURVE 11
#define XWACOM_PARAM_RAWFILTER  12
#define XWACOM_PARAM_MODE       13
#define XWACOM_PARAM_SPEEDLEVEL 14
#define XWACOM_PARAM_CLICKFORCE 15
#define XWACOM_PARAM_ACCEL      16
#define XWACOM_PARAM_XYDEFAULT  65
#define XWACOM_PARAM_FILEMODEL  100
#define XWACOM_PARAM_FILEOPTION 101
#define XWACOM_PARAM_GIMP       102

WacomModule gWacomModule =
{
    0,			/* debug level */
    wacom_map,		/* key map */
    identification,	/* version */
    { NULL },		/* input driver pointer */
    /* device procedures */
    xf86WcmDevOpen,
    xf86WcmDevReadInput,
    xf86WcmDevControlProc,
    xf86WcmDevClose,
    xf86WcmDevProc,
    xf86WcmDevChangeControl,
    xf86WcmDevSwitchMode,
    xf86WcmDevConvert,
    xf86WcmDevReverseConvert,
};

/******************************************************************************
 * WacomDeviceClass
 *****************************************************************************/

struct _WacomDeviceClass
{
    Bool (*Detect)(LocalDevicePtr local); /* detect device */
    Bool (*Init)(LocalDevicePtr local);   /* initialize device */
    void (*Read)(LocalDevicePtr local);   /* reads device */
};

static WacomDeviceClass gWacomSerialDevice =
{
    serialDetect,
    serialInit,
    xf86WcmReadPacket,
};

static WacomDeviceClass gWacomISDV4Device =
{
    isdv4Detect,
    isdv4Init,
    xf86WcmReadPacket,
};

#ifdef LINUX_INPUT
static WacomDeviceClass gWacomUSBDevice =
{
    usbDetect,
    usbInit,
    xf86WcmReadPacket,
};

static WacomModel usbUnknown =
{
    "Unknown USB",
    usbInitProtocol5,     /* assume the best */
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    NULL,                 /* input filtering not needed */
};

static WacomModel usbPenPartner =
{
    "USB PenPartner",
    usbInitProtocol4,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterCoord,   /* input filtering */
};

static WacomModel usbGraphire =
{
    "USB Graphire",
    usbInitProtocol4,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterCoord,   /* input filtering */
};

static WacomModel usbGraphire2 =
{
    "USB Graphire2",
    usbInitProtocol4,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterCoord,   /* input filtering */
};

static WacomModel usbGraphire3 =
{
    "USB Graphire3",
    usbInitProtocol4,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterCoord,   /* input filtering */
};

static WacomModel usbCintiq =
{
    "USB Cintiq",
    usbInitProtocol4,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterCoord,   /* input filtering */
};

static WacomModel usbCintiqPartner =
{
    "USB CintiqPartner",
    usbInitProtocol4,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterCoord,   /* input filtering */
};

static WacomModel usbIntuos =
{
    "USB Intuos",
    usbInitProtocol5,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterIntuos,  /* input filtering recommended */
};

static WacomModel usbIntuos2 =
{
    "USB Intuos2",
    usbInitProtocol5,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterIntuos,  /* input filtering recommended */
};

static WacomModel usbVolito =
{
    "USB Volito",
    usbInitProtocol4,
    NULL,                 /* resolution not queried */
    usbGetRanges,
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    usbParse,
    xf86WcmFilterCoord,   /* input filtering */
};
#endif /* LINUX_INPUT */

static WacomDeviceClass* wcmDeviceClasses[] =
{
    /* Current USB implementation requires LINUX_INPUT */
#ifdef LINUX_INPUT
    &gWacomUSBDevice,
#endif

     &gWacomISDV4Device,
     &gWacomSerialDevice,
     NULL
};

#ifdef LINUX_INPUT
/*****************************************************************************
 * usbDetect --
 *   Test if the attached device is USB.
 ****************************************************************************/

static Bool usbDetect(LocalDevicePtr local)
{
    int version;
    int err;

    DBG(1, ErrorF("usbDetect\n"));

    SYSCALL(err = ioctl(local->fd, EVIOCGVERSION, &version));

    if (!err) {
	ErrorF("%s Wacom Kernel Input driver version is %d.%d.%d\n",
                                XCONFIG_PROBED, version >> 16,
                                (version >> 8) & 0xff, version & 0xff);
	return 1;
    }

    return 0;
}

/*****************************************************************************
 * usbInit --
 ****************************************************************************/

static Bool usbInit(LocalDevicePtr local)
{
    short sID[4];
    char id[BUFFER_SIZE];
    WacomModelPtr model = NULL;
    WacomDevicePtr priv = (WacomDevicePtr)local->private;
    WacomCommonPtr common = priv->common;

    DBG(1, ErrorF("initializing USB tablet\n"));

    /* fetch vendor, product, and model name */
    ioctl(local->fd, EVIOCGID, sID);
    ioctl(local->fd, EVIOCGNAME(sizeof(id)), id);

    /* vendor is wacom */
    if (sID[1] == 0x056A) {
 	/* switch on product */
	switch (sID[2]) {
	    case 0x00: /* PenPartner */
		model = &usbPenPartner; break;

	    case 0x10: /* Graphire */
		model = &usbGraphire; break;

	    case 0x11: /* Graphire2 4x5 */
	    case 0x12: /* Graphire2 5x7 */
		model = &usbGraphire2; break;

	    case 0x13: /* Graphire2 4x5 */
	    case 0x14: /* Graphire2 6x8 */
		model = &usbGraphire3; break;

	    case 0x20: /* Intuos 4x5 */
	    case 0x21: /* Intuos 6x8 */
	    case 0x22: /* Intuos 9x12 */
	    case 0x23: /* Intuos 12x12 */
	    case 0x24: /* Intuos 12x18 */
		model = &usbIntuos; break;

	    case 0x03: /* PTU600 */
		model = &usbCintiqPartner; break;

	    case 0x30: /* PL400 */
	    case 0x31: /* PL500 */
	    case 0x32: /* PL600 */
 	    case 0x33: /* PL600SX */
	    case 0x34: /* PL550 */
	    case 0x35: /* PL800 */
		model = &usbCintiq; break;

	    case 0x41: /* Intuos2 4x5 */
	    case 0x42: /* Intuos2 6x8 */
	    case 0x43: /* Intuos2 9x12 */
	    case 0x44: /* Intuos2 12x12 */
	    case 0x45: /* Intuos2 12x18 */
	    case 0x47: /* Intuos2 6x8 (verified in the field) */
		model = &usbIntuos2; break;

	    case 0x60: /* Volito */
		model = &usbVolito; break;
	}
    }

    if (!model) model = &usbUnknown;

    return xf86WcmInitTablet(common,model,local->fd,id,0.0);
}

static void usbInitProtocol5(WacomCommonPtr common, int fd, const char* id,
        float version)
{
    DBG(2, ErrorF("detected a protocol 5 model (%s)\n",id));
    common->wcmResolX = common->wcmResolY = 2540;
    common->wcmProtocolLevel = 5;
    common->wcmChannelCnt = 2;
    common->wcmPktLength = sizeof(struct input_event);
}

static void usbInitProtocol4(WacomCommonPtr common, int fd, const char* id,
        float version)
{
    DBG(2, ErrorF("detected a protocol 4 model (%s)\n",id));
    common->wcmResolX = common->wcmResolY = 1016;
    common->wcmProtocolLevel = 4;
    common->wcmPktLength = sizeof(struct input_event);
}

#define BIT(x) (1<<(x))
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define ISBITSET(x,y) ((x)[LONG(y)] & BIT(y))
#define OFF(x)   ((x)%BITS_PER_LONG)
#define LONG(x)  ((x)/BITS_PER_LONG)

static int usbGetRanges(WacomCommonPtr common, int fd)
{
    int nValues[5];
    unsigned long ev[NBITS(EV_MAX)];
    unsigned long abs[NBITS(ABS_MAX)];

    if (ioctl(fd, EVIOCGBIT(0 /*EV*/, sizeof(ev)), ev) < 0) {
	ErrorF("WACOM: unable to ioctl event bits.\n");
	return !Success;
    }

    /* absolute values */
    if (ISBITSET(ev,EV_ABS)) {
	if (ioctl(fd, EVIOCGBIT(EV_ABS,sizeof(abs)),abs) < 0) {
	    ErrorF("WACOM: unable to ioctl abs bits.\n");
	    return !Success;
	}

	/* max x */
	if (common->wcmMaxX == 0) {
	    if (ioctl(fd, EVIOCGABS(ABS_X), nValues) < 0) {
		ErrorF("WACOM: unable to ioctl xmax value.\n");
		return !Success;
	    }
	    common->wcmMaxX = nValues[2];
 	}

 	/* max y */
 	if (common->wcmMaxY == 0) {
	    if (ioctl(fd, EVIOCGABS(ABS_Y), nValues) < 0) {
		ErrorF("WACOM: unable to ioctl ymax value.\n");
		return !Success;
	    }
	    common->wcmMaxY = nValues[2];
	}

	/* max z cannot be configured */
	if (ioctl(fd, EVIOCGABS(ABS_PRESSURE), nValues) < 0) {
	    ErrorF("WACOM: unable to ioctl press max value.\n");
	    return !Success;
	}
	common->wcmMaxZ = nValues[2];
    }

    return Success;
}

static int usbParse(WacomCommonPtr common, const unsigned char* data)
{
    usbParseEvent(common,(const struct input_event*)data);
    return common->wcmPktLength;
}

static void usbParseEvent(WacomCommonPtr common,
        const struct input_event* event)
{
    int i, serial, channel;

    /* store events until we receive the MSC_SERIAL containing
     * the serial number; without it we cannot determine the
     * correct channel. */

    /* space left? bail if not. */
    if (common->wcmEventCnt >= (sizeof(common->wcmEvents)/sizeof(*common->wcmEvents)))
    {
	DBG(1, ErrorF("usbParse: Exceeded event queue (%d)\n",
                                common->wcmEventCnt));
	common->wcmEventCnt = 0;
	return;
    }

    /* save it for later */
    common->wcmEvents[common->wcmEventCnt++] = *event;

    /* packet terminated by MSC_SERIAL on kernel 2.4 and SYN_REPORT on kernel 2.5 */
    if ((event->type != EV_MSC) || (event->code != MSC_SERIAL)) {
#ifdef EV_SYN
	/* none serial number tools fall here */
	if ((event->type == EV_SYN) && (event->code == SYN_REPORT)) {
	    usbParseChannel(common,0,0);
	    common->wcmEventCnt = 0;
	}
#endif
	return;
    }
    /* serial number is key for channel */
    serial = event->value;
    channel = -1;

    /* one channel only? must be it. */
    if (common->wcmChannelCnt == 1) channel = 0;
    else {		/* otherwise, find the channel */
	/* find existing channel */
	for (i=0; i<common->wcmChannelCnt; ++i) {
	    if (common->wcmChannel[i].work.serial_num == serial) {
		channel = i;
		break;
	    }
	}

	/* find an empty channel */
	if (channel < 0) {
	    for (i=0; i<common->wcmChannelCnt; ++i) {
		if (common->wcmChannel[i].work.proximity == 0) {
		    /* clear out channel */
		    memset(&common->wcmChannel[i],0, sizeof(WacomChannel));
		    channel = i;
		    break;
		}
	    }
	}

	/* fresh out of channels */
	if (channel < 0) {
	    /* this should never happen in normal use */
	    DBG(1, ErrorF("usbParse: Exceeded channel count; ignoring.\n"));
	    return;
	}
    }

    usbParseChannel(common,channel,serial);

    common->wcmEventCnt = 0;
}

static void usbParseChannel(WacomCommonPtr common, int channel, int serial)
{
    int i;
    WacomDeviceState* ds;
    struct input_event* event;

#   define MOD_BUTTONS(bit, value) do { \
		ds->buttons = (((value) != 0) ? \
                (ds->buttons | (bit)) : (ds->buttons & ~(bit))); \
                } while (0)

    /* all USB data operates from previous context except relative values*/
    ds = &common->wcmChannel[channel].work;
    ds->relwheel = 0;
    ds->serial_num = serial;

    /* loop through all events in group */
    for (i=0; i<common->wcmEventCnt; ++i)
    {
	event = common->wcmEvents + i;

	DBG(11, ErrorF("usbParseChannel event[%d]->type=%d code=%d value=%d\n", 
		i, event->type, event->code, event->value));

	/* absolute events */
	if (event->type == EV_ABS) {
	    if (event->code == ABS_X)
		ds->x = event->value;
	    else if (event->code == ABS_Y)
		ds->y = event->value;
	    else if (event->code == ABS_RZ)
		ds->rotation = event->value;
	    else if (event->code == ABS_TILT_X)
		ds->tiltx = event->value - 64;
	    else if (event->code ==  ABS_TILT_Y)
		ds->tilty = event->value - 64;
	    else if (event->code == ABS_PRESSURE) {
		ds->pressure = event->value;
		MOD_BUTTONS (1, event->value > common->wcmThreshold ? 1 : 0);
		/* pressure button should be downstream */
	    }
	    else if (event->code == ABS_DISTANCE)
		ds->distance = event->value;
	    else if (event->code == ABS_WHEEL)
		ds->abswheel = event->value;
	    else if (event->code == ABS_THROTTLE)
		ds->throttle = event->value;
	} else if (event->type == EV_REL) {
	    if (event->code == REL_WHEEL)
		ds->relwheel = event->value;
	    else {
		ErrorF("wacom: rel event recv'd (%d)!\n", event->code);
	    }
	} else if (event->type == EV_KEY) {
	    if ((event->code == BTN_TOOL_PEN) || (event->code == BTN_TOOL_PENCIL) ||
		(event->code == BTN_TOOL_BRUSH) || (event->code == BTN_TOOL_AIRBRUSH)) {
		ds->device_type = STYLUS_ID;
		ds->proximity = (event->value != 0);
		DBG(6, ErrorF("USB stylus detected %x\n", event->code));
	    } else if (event->code == BTN_TOOL_RUBBER) {
		ds->device_type = ERASER_ID;
		ds->proximity = (event->value != 0);
		if (ds->proximity) ds->proximity = ERASER_PROX;
		DBG(6, ErrorF("USB eraser detected %x\n", event->code));
	    } else if ((event->code == BTN_TOOL_MOUSE) || (event->code == BTN_TOOL_LENS)) {
		DBG(6, ErrorF("USB mouse detected %x\n", event->code));
		ds->device_type = CURSOR_ID;
		ds->proximity = (event->value != 0);
	    } else if (event->code == BTN_TOUCH) {
		/* we use the pressure to determine the button 1 for now */
	    } else if ((event->code == BTN_STYLUS) || (event->code == BTN_MIDDLE)) {
		MOD_BUTTONS (2, event->value);
	    } else if ((event->code == BTN_STYLUS2) || (event->code == BTN_RIGHT)) {
		 MOD_BUTTONS (4, event->value);
	    } else if (event->code == BTN_LEFT)
		MOD_BUTTONS (1, event->value);
	    else if (event->code == BTN_SIDE)
		MOD_BUTTONS (8, event->value);
	    else if (event->code == BTN_EXTRA)
		MOD_BUTTONS (16, event->value);
	}
    } /* next event */

    /* dispatch event */
    xf86WcmEvent(common, channel, ds);
}
#endif /* LINUX_INPUT */

static WacomModel serialIntuos =
{
    "Serial Intuos",
    serialInitIntuos,
    NULL,           /* resolution not queried */
    serialGetRanges,
    serialResetIntuos,
    NULL,           /* tilt automatically enabled */
    NULL,           /* suppress implemented in software */
    serialSetLinkSpeedIntuos,
    serialStartTablet,
    serialParseProtocol5,
    xf86WcmFilterIntuos,
};

static WacomModel serialIntuos2 =
{
    "Serial Intuos2",
    serialInitIntuos2,
    NULL,                 /* resolution not queried */
    serialGetRanges,
    serialResetIntuos,    /* same as Intuos */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    serialSetLinkSpeedProtocol5,
    serialStartTablet,
    serialParseProtocol5,
    xf86WcmFilterIntuos,
};

static WacomModel serialCintiq =
{
    "Serial Cintiq",
    serialInitCintiq,
    serialGetResolution,
    serialGetRanges,
    serialResetCintiq,
    serialEnableTiltProtocol4,
    serialEnableSuppressProtocol4,
    NULL,               /* link speed cannot be changed */
    serialStartTablet,
    serialParseCintiq,
    xf86WcmFilterCoord,
};

static WacomModel serialPenPartner =
{
    "Serial PenPartner",
    serialInitPenPartner,
    NULL,               /* resolution not queried */
    serialGetRanges,
    serialResetPenPartner,
    serialEnableTiltProtocol4,
    serialEnableSuppressProtocol4,
    NULL,              /* link speed cannot be changed */
    serialStartTablet,
    serialParseProtocol4,
    xf86WcmFilterCoord,
};

static WacomModel serialGraphire =
{
    "Serial Graphire",
    serialInitGraphire,
    NULL,                     /* resolution not queried */
    NULL,                     /* ranges not supported */
    serialResetPenPartner,    /* functionally very similar */
    serialEnableTiltProtocol4,
    serialEnableSuppressProtocol4,
    NULL,                    /* link speed cannot be changed */
    serialStartTablet,
    serialParseGraphire,
    xf86WcmFilterCoord,
};

static WacomModel serialProtocol4 =
{
    "Serial UD",
    serialInitProtocol4,
    serialGetResolution,
    serialGetRanges,
    serialResetProtocol4,
    serialEnableTiltProtocol4,
    serialEnableSuppressProtocol4,
    NULL,               /* link speed cannot be changed */
    serialStartTablet,
    serialParseProtocol4,
     xf86WcmFilterCoord,
};

/*
 ***************************************************************************
 *
 * xf86WcmSendRequest --
 *   send a request and wait for the answer.
 *   the answer must begin with the first two chars of the request.
 *   The last character in the answer string is replaced by a \0.
 *
 ***************************************************************************
 */
static char *
xf86WcmSendRequest(int	fd,
	     	char	*request,
	     	char	*answer,
		 int	maxlen)
{
    int	len, nr;
    int	maxtry = MAXTRY;

    if (maxlen < 3)
	return NULL;
  
    /* send request string */
    do {
	SYSCALL(len = xf86WcmWrite(fd, request, strlen(request)));
	if ((len == -1) && (errno != EAGAIN)) {
	    ErrorF("Wacom xf86WcmWrite error : %s", strerror(errno));
	    return NULL;
	}
	maxtry--;
    } while ((len == -1) && maxtry);

    if (maxtry == 0) {
	ErrorF("Wacom unable to xf86WcmWrite request string '%s' after %d tries\n", request, MAXTRY);
	return NULL;
    }
  
    do {
	maxtry = MAXTRY;
    
	/* Read the first byte of the answer which must be equal to the first
	 * byte of the request.
	 */
	do {    
	    if ((nr = xf86WcmWaitForTablet(fd)) > 0) {
		SYSCALL(nr = xf86WcmRead(fd, answer, 1));
		if ((nr == -1) && (errno != EAGAIN)) {
		    ErrorF("Wacom xf86WcmRead error : %s\n", strerror(errno));
		    return NULL;
		}
		DBG(10, ErrorF("%c err=%d [0]\n", answer[0], nr));
	    }
	    maxtry--;  
	} while ((answer[0] != request[0]) && maxtry);

	if (maxtry == 0) {
	    ErrorF("Wacom unable to read first byte of request '%c%c' answer after %d tries\n",
		   request[0], request[1], MAXTRY);
	    return NULL;
	}

	/* Read the second byte of the answer which must be equal to the second
	 * byte of the request.
	 */
	do {    
	    maxtry = MAXTRY;
	    do {    
		if ((nr = xf86WcmWaitForTablet(fd)) > 0) {
		    SYSCALL(nr = xf86WcmRead(fd, answer+1, 1));
		    if ((nr == -1) && (errno != EAGAIN)) {
			ErrorF("Wacom xf86WcmRead error : %s\n", strerror(errno));
			return NULL;
		    }
		    DBG(10, ErrorF("%c err=%d [1]\n", answer[1], nr));
		}
		maxtry--;  
	    } while ((nr <= 0) && maxtry);
      
	    if (maxtry == 0) {
		ErrorF("Wacom unable to read second byte of request '%c%c' answer after %d tries\n",
		       request[0], request[1], MAXTRY);
		return NULL;
	    }

	    if (answer[1] != request[1])
		answer[0] = answer[1];
      
	} while ((answer[0] == request[0]) &&
		 (answer[1] != request[1]));

    } while ((answer[0] != request[0]) &&
	     (answer[1] != request[1]));

    /* Read until we don't get anything or timeout.
     */
    len = 2;
    maxtry = MAXTRY;
    do {    
	do {    
	    if ((nr = xf86WcmWaitForTablet(fd)) > 0) {
		SYSCALL(nr = xf86WcmRead(fd, answer+len, 1));
		if ((nr == -1) && (errno != EAGAIN)) {
		    ErrorF("Wacom xf86WcmRead error : %s\n", strerror(errno));
		    return NULL;
		}
		DBG(10, ErrorF("%c err=%d [%d]\n", answer[len], nr, len));
	    }
	    else {
		if (len == 2) {
		   DBG(10, ErrorF("timeout remains %d tries\n", maxtry));
		   maxtry--;
		}
	    }
	} while ((nr <= 0) && len == 2 && maxtry);

	if (nr > 0) {
	    len += nr;
		if (len >= (maxlen - 1))
			return NULL;
	}
	
	if (maxtry == 0) {
	    ErrorF("Wacom unable to read last byte of request '%c%c' answer after %d tries\n",
		   request[0], request[1], MAXTRY);
	    break;
	}
    } while (nr > 0);

    if (len <= 3)
	return NULL;
    
    answer[len-1] = '\0';
  
    return answer;
}

static Bool serialDetect(LocalDevicePtr pDev)
{
    return 1;
}

static Bool serialInit(LocalDevicePtr local)
{
    int err;
    WacomCommonPtr common = ((WacomDevicePtr)(local->private))->common;

    DBG(1, ErrorF("initializing serial tablet\n"));

    /* Set the speed of the serial link to 38400 */
    if (xf86WcmSetSerialSpeed(local->fd, 38400) < 0)
	return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));

    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 250 mSecs */
    if (xf86WcmWait(250)) return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET)));
    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 75 mSecs */
    if (xf86WcmWait(75)) return !Success;

    /* Set the speed of the serial link to 19200 */
    if (xf86WcmSetSerialSpeed(local->fd, 19200) < 0) return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));

    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 250 mSecs */
    if (xf86WcmWait(250)) return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET)));
    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 75 mSecs */
    if (xf86WcmWait(75)) return !Success;

    /* Set the speed of the serial link to 9600 */
    if (xf86WcmSetSerialSpeed(local->fd, 9600) < 0) return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));
    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 250 mSecs */
    if (xf86WcmWait(250)) return !Success;

    SYSCALL(err = xf86WcmWrite(local->fd, WC_STOP, strlen(WC_STOP)));
    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 30 mSecs */
    if (xf86WcmWait(30)) return !Success;

    xf86WcmFlushTablet(local->fd);

    if (serialInitTablet(common,local->fd) == !Success) {
	SYSCALL(xf86WcmClose(local->fd));
	local->fd = -1;
	return !Success;
    }

    return Success;
}

/*****************************************************************************
 * serialInitTablet --
 *   Initialize the tablet
 ****************************************************************************/

static int serialInitTablet(WacomCommonPtr common, int fd)
{
    int			loop, idx;
    char		id[BUFFER_SIZE];
    float		version;

    WacomModelPtr	model = NULL;

    /* if model is forced, initialize */
    if (model != NULL)
    {
	id[0] = '\0';
	version = 0.0F;
    }

    /* otherwise, query and initialize */
    else
    {
	DBG(2, ErrorF("reading model\n"));
	if (!xf86WcmSendRequest(fd, WC_MODEL, id, sizeof(id))) return !Success;

 	DBG(2, ErrorF("%s\n", id));

	if (xf86Verbose) ErrorF("%s Wacom tablet model : %s\n", XCONFIG_PROBED, id+2);

	/* Answer is in the form ~#Tablet-Model VRom_Version
	 * look for the first V from the end of the string
	 * this seems to be the better way to find the version
	 * of the ROM */
	for(loop=strlen(id); loop>=0 && *(id+loop) != 'V'; loop--);
	for(idx=loop; idx<strlen(id) && *(id+idx) != '-'; idx++); *(id+idx) = '\0';

	/* Extract version numbers */
	sscanf(id+loop+1, "%f", &version);

	/* Detect tablet model based on identifier */
	if (id[2] == 'G' && id[3] == 'D')
	    model = &serialIntuos;
	else if (id[2] == 'X' && id[3] == 'D')
	   model = &serialIntuos2;
	else if (id[2] == 'P' && id[3] == 'L')
	    model = &serialCintiq;
	else if (id[2] == 'C' && id[3] == 'T')
 	    model = &serialPenPartner;
	else if (id[2] == 'E' && id[3] == 'T')
 	    model = &serialGraphire;
 	else
	   model = &serialProtocol4;
    }
    return xf86WcmInitTablet(common,model,fd,id,version);
}

static int serialParseGraphire(WacomCommonPtr common,
			const unsigned char* data)
{
    int n;
    WacomDeviceState* last = &common->wcmChannel[0].valid.state;
    WacomDeviceState* ds;

    /* positive value is skip */
    if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

    /* pick up where we left off, minus relative values */
    ds = &common->wcmChannel[0].work;
    RESET_RELATIVE(*ds);

    /* get pressure */
    ds->pressure = ((data[6]&ZAXIS_BITS) << 2 ) +
                ((data[3]&ZAXIS_BIT) >> 1) +
                ((data[3]&PROXIMITY_BIT) >> 6) +
                ((data[6]&ZAXIS_SIGN_BIT) ? 0 : 0x100);

    /* get buttons */
    ds->buttons = (data[3] & 0x38) >> 3;

    /* requires button info, so it goes down here. */
    serialParseP4Common(common,data,last,ds);

    /* handle relative wheel for non-stylus device */
    if (ds->device_type == CURSOR_ID)
    {
	ds->relwheel = (data[6] & 0x30) >> 4;
	if (data[6] & 0x40) ds->relwheel = -ds->relwheel;

    }

    xf86WcmEvent(common,0,ds);
    return common->wcmPktLength;
}

static int serialParseCintiq(WacomCommonPtr common,
			const unsigned char* data)
{
    int n;
    WacomDeviceState* last = &common->wcmChannel[0].valid.state;
    WacomDeviceState* ds;

    /* positive value is skip */
    if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

    /* pick up where we left off, minus relative values */
    ds = &common->wcmChannel[0].work;
    RESET_RELATIVE(*ds);

    /* get pressure */
    if (common->wcmMaxZ == 255)
    {
	ds->pressure = ((data[6] & ZAXIS_BITS) << 1 ) |
                        ((data[3] & ZAXIS_BIT) >> 2) |
                        ((data[6] & ZAXIS_SIGN_BIT) ? 0 : 0x80);
    }
    else
    {
	/* PL550, PL800, and Graphire apparently */
	ds->pressure = ((data[6]&ZAXIS_BITS) << 2 ) +
                        ((data[3]&ZAXIS_BIT) >> 1) +
                        ((data[3]&PROXIMITY_BIT) >> 6) +
                        ((data[6]&ZAXIS_SIGN_BIT) ? 0 : 0x100);
    }

    /* get buttons */
    ds->buttons = (data[3] & 0x38) >> 3;

    /* requires button info, so it goes down here. */
    serialParseP4Common(common,data,last,ds);

    xf86WcmEvent(common,0,ds);
    return common->wcmPktLength;
}

static int serialParseProtocol4(WacomCommonPtr common,
			const unsigned char* data)
{
    int n;
    WacomDeviceState* last = &common->wcmChannel[0].valid.state;
    WacomDeviceState* ds;

    /* positive value is skip */
    if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

    /* pick up where we left off, minus relative values */
    ds = &common->wcmChannel[0].work;
    RESET_RELATIVE(*ds);

    /* get pressure */
    if (common->wcmMaxZ == 255)
	ds->pressure = ((data[6] & ZAXIS_BITS) << 1 ) |
                        ((data[3] & ZAXIS_BIT) >> 2) |
                        ((data[6] & ZAXIS_SIGN_BIT) ? 0 : 0x80);

    else
	ds->pressure = (data[6] & ZAXIS_BITS) |
                        (data[6] & ZAXIS_SIGN_BIT) ? 0 : 0x40;

    /* get button state */
    ds->buttons = (data[3] & BUTTONS_BITS) >> 3;

    /* requires button info, so it goes down here. */
    serialParseP4Common(common,data,last,ds);

    xf86WcmEvent(common,0,ds);
    return common->wcmPktLength;
}

static int serialParseProtocol5(WacomCommonPtr common,
			const unsigned char* data)
{
    int n;
    int have_data=0;
    int channel;
    WacomDeviceState* ds;

    /* positive value is skip */
    if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

    /* Protocol 5 devices support 2 data channels */
    channel = data[0] & 0x01;

    /* pick up where we left off, minus relative values */
    ds = &common->wcmChannel[channel].work;
    RESET_RELATIVE(*ds);

    DBG(7, ErrorF("packet header = 0x%x\n", (unsigned int)data[0]));

    /* Device ID packet */
    if ((data[0] & 0xfc) == 0xc0) {
	/* start from scratch */
	memset(ds, 0, sizeof(*ds));

	ds->proximity = 1;
	ds->device_id = (((data[1] & 0x7f) << 5) |
                                ((data[2] & 0x7c) >> 2));
	ds->serial_num = (((data[2] & 0x03) << 30) |
                                ((data[3] & 0x7f) << 23) |
                                ((data[4] & 0x7f) << 16) |
                                ((data[5] & 0x7f) << 9) |
                                ((data[6] & 0x7f) << 2) |
                                ((data[7] & 0x60) >> 5));

	if ((ds->device_id & 0xf06) != 0x802) ds->discard_first = 1;

	if (STYLUS_TOOL(ds))
	    ds->device_type = STYLUS_ID;
	else if (CURSOR_TOOL(ds))
	    ds->device_type = CURSOR_ID;
	else
	    ds->device_type = ERASER_ID;

	DBG(6, ErrorF("device_id=0x%x serial_num=%u type=%s\n",
                        ds->device_id, ds->serial_num,
                        (ds->device_type == STYLUS_ID) ? "stylus" :
                        (ds->device_type == CURSOR_ID) ? "cursor" :
                        "eraser"));
    } else if ((data[0] & 0xfe) == 0x80) {	/* Out of proximity packet */
	ds->proximity = 0;
	have_data = 1;
    }

    /* General pen packet or eraser packet or airbrush first packet
     * airbrush second packet */
    else if (((data[0] & 0xb8) == 0xa0) || ((data[0] & 0xbe) == 0xb4)) {
	ds->x = (((data[1] & 0x7f) << 9) | ((data[2] & 0x7f) << 2) |
                                ((data[3] & 0x60) >> 5));
	ds->y = (((data[3] & 0x1f) << 11) | ((data[4] & 0x7f) << 4) |
                                ((data[5] & 0x78) >> 3));
	if ((data[0] & 0xb8) == 0xa0) {
	    ds->pressure = (((data[5] & 0x07) << 7) | (data[6] & 0x7f));
	    ds->buttons = (((data[0]) & 0x06) | (ds->pressure >= common->wcmThreshold));
	    /* pressure button should go down stream */
	} else {
	    /* 10 bits for absolute wheel position */
	    ds->abswheel = (((data[5] & 0x07) << 7) | (data[6] & 0x7f));
	}
	ds->tiltx = (data[7] & TILT_BITS);
	ds->tilty = (data[8] & TILT_BITS);
	if (data[7] & TILT_SIGN_BIT)
	    ds->tiltx -= (TILT_BITS + 1);
	if (data[8] & TILT_SIGN_BIT)
	    ds->tilty -= (TILT_BITS + 1);
	ds->proximity = (data[0] & PROXIMITY_BIT);
	have_data = 1;
    } /* end pen packet */

    /* 4D mouse 1st packet or Lens cursor packet or 2D mouse packet*/
    else if (((data[0] & 0xbe) == 0xa8) || ((data[0] & 0xbe) == 0xb0)) {
	ds->x = (((data[1] & 0x7f) << 9) | ((data[2] & 0x7f) << 2) |
                                ((data[3] & 0x60) >> 5));
	ds->y = (((data[3] & 0x1f) << 11) | ((data[4] & 0x7f) << 4) |
                                ((data[5] & 0x78) >> 3));
	ds->tilty = 0;

	if (MOUSE_4D(ds)) {	/* 4D mouse */
	    ds->throttle = (((data[5] & 0x07) << 7) | (data[6] & 0x7f));
	    if (data[8] & 0x08) ds->throttle = -ds->throttle;
	    ds->buttons = (((data[8] & 0x70) >> 1) | (data[8] & 0x07));
	    have_data = !ds->discard_first;
	} else if (LENS_CURSOR(ds)) {	/* Lens cursor */
	    ds->buttons = data[8];
	    have_data = 1;
	} else if (MOUSE_2D(ds)) {	/* 2D mouse */
	    ds->buttons = (data[8] & 0x1C) >> 2;
	    ds->relwheel = -(data[8] & 1) + ((data[8] & 2) >> 1);
	    have_data = 1; /* must send since relwheel is reset */
	}

	ds->proximity = (data[0] & PROXIMITY_BIT);
    } /* end 4D mouse 1st packet */
    else if ((data[0] & 0xbe) == 0xaa) { /* 4D mouse 2nd packet */
	ds->x = (((data[1] & 0x7f) << 9) | ((data[2] & 0x7f) << 2) |
                        ((data[3] & 0x60) >> 5));
	ds->y = (((data[3] & 0x1f) << 11) | ((data[4] & 0x7f) << 4) |
                        ((data[5] & 0x78) >> 3));
	ds->rotation = (((data[6] & 0x0f) << 7) | (data[7] & 0x7f));
	if (ds->rotation < 900) ds->rotation = -ds->rotation;
	else ds->rotation = 1799 - ds->rotation;
	ds->proximity = (data[0] & PROXIMITY_BIT);
	have_data = 1;
	ds->discard_first = 0;
    } else {
	DBG(10, ErrorF("unknown wacom V packet 0x%x\n", data[0]));
    }

    /* if new data is available, send it */
    if (have_data) {
	xf86WcmEvent(common,channel,ds);
    } else {	/* otherwise, initialize channel and wait for next packet */
 	common->wcmChannel[channel].pDev = NULL;
    }
    return common->wcmPktLength;
}

/*****************************************************************************
 * Model-specific functions
 ****************************************************************************/

static void serialInitIntuos(WacomCommonPtr common, int fd,
			const char* id, float version)
{
    DBG(2, ErrorF("detected an Intuos model\n"));

    common->wcmProtocolLevel = 5;
    common->wcmVersion = version;

    common->wcmMaxZ = 1023;   /* max Z value */
    common->wcmResolX = 2540; /* tablet X resolution in points/inch */
    common->wcmResolY = 2540; /* tablet Y resolution in points/inch */
    common->wcmPktLength = 9; /* length of a packet */
    common->wcmFlags |= TILT_ENABLED_FLAG;
}

static void serialInitIntuos2(WacomCommonPtr common, int fd,
			const char* id, float version)
{
    DBG(2, ErrorF("detected an Intuos2 model\n"));

    common->wcmProtocolLevel = 5;
    common->wcmVersion = version;

    common->wcmMaxZ = 1023;       /* max Z value */
    common->wcmResolX = 2540;     /* tablet X resolution in points/inch */
    common->wcmResolY = 2540;     /* tablet Y resolution in points/inch */
    common->wcmPktLength = 9;     /* length of a packet */
    common->wcmFlags |= TILT_ENABLED_FLAG;
}

static void serialInitCintiq(WacomCommonPtr common, int fd,
			const char* id, float version)
{
    DBG(2, ErrorF("detected a Cintiq model\n"));

    common->wcmProtocolLevel = 4;
    common->wcmPktLength = 7;
    common->wcmVersion = version;

    if (id[5] == '2') {
	/* PL-250  */
	if ( id[6] == '5' ) {
	    /* PL-250  */
	    common->wcmMaxZ = 255;
	} else {
	    /* PL-270  */
	    common->wcmMaxZ = 255;
	}
    } else if (id[5] == '3') {
	/* PL-300  */
	common->wcmMaxZ = 255;
    } else if (id[5] == '4') {
	/* PL-400  */
	common->wcmMaxZ = 255;
    } else if (id[5] == '5') {
	/* PL-550  */
	if ( id[6] == '5' ) {
	    common->wcmMaxZ = 511;
	} else {
	    /* PL-500  */
	    common->wcmMaxZ = 255;
	}
    } else if (id[5] == '6') {
	/* PL-600SX  */
	if ( id[8] == 'S' ) {
	    common->wcmMaxZ = 255;
	} else {
	    /* PL-600  */
	    common->wcmMaxZ = 255;
	}
    } else if (id[5] == '8') {
 	/* PL-800  */
	common->wcmMaxZ = 511;
    }

    common->wcmResolX = 508; /* tablet X resolution in points/inch */
    common->wcmResolY = 508; /* tablet Y resolution in points/inch */
}

static void serialInitPenPartner(WacomCommonPtr common, int fd,
			const char* id, float version)
{
    DBG(2, ErrorF("detected a PenPartner model\n"));

    common->wcmProtocolLevel = 4;
    common->wcmPktLength = 7;
    common->wcmVersion = version;

    common->wcmMaxZ = 256;
    common->wcmResolX = 1000; /* tablet X resolution in points/inch */
    common->wcmResolY = 1000; /* tablet Y resolution in points/inch */
}

static void serialInitGraphire(WacomCommonPtr common, int fd,
			const char* id, float version)
{
    DBG(2, ErrorF("detected a Graphire model\n"));

    common->wcmProtocolLevel = 4;
    common->wcmPktLength = 7;
    common->wcmVersion = version;

    /* Graphire models don't answer WC_COORD requests */
    common->wcmMaxX = 5103;
    common->wcmMaxY = 3711;
    common->wcmMaxZ = 512;
    common->wcmResolX = 1000; /* tablet X resolution in points/inch */
    common->wcmResolY = 1000; /* tablet Y resolution in points/inch */
}

static void serialInitProtocol4(WacomCommonPtr common, int fd,
			const char* id, float version)
{
    DBG(2, ErrorF("detected a Protocol4 model\n"));

    common->wcmProtocolLevel = 4;
    common->wcmPktLength = 7;
    common->wcmVersion = version;

    /* If no maxZ is set, determine from version */
    if (!common->wcmMaxZ) {
	/* the rom version determines the max z */
	if (version >= (float)1.2) common->wcmMaxZ = 255;
	else common->wcmMaxZ = 120;
    }
}

static void serialGetResolution(WacomCommonPtr common, int fd)
{
    int a, b;
    char buffer[BUFFER_SIZE], header[BUFFER_SIZE];

    if (!(common->wcmResolX && common->wcmResolY)) {
	DBG(2, ErrorF("Requesting resolution from device\n"));
	if (xf86WcmSendRequest(fd, WC_CONFIG, buffer, sizeof(buffer)))
	{
	    DBG(2, ErrorF("%s\n", buffer));
	    /* The header string is simply a place to put the
	     * unwanted config header don't use buffer+xx because
	     * the header size varies on different tablets */

	    if (sscanf(buffer, "%[^,],%d,%d,%d,%d", header,
 			&a, &b, &common->wcmResolX, &common->wcmResolY) == 5) {
		DBG(6, ErrorF("WC_CONFIG Header = %s\n", header));
	    } else {
		ErrorF("WACOM: unable to parse resolution. Using default.\n");
		common->wcmResolX = common->wcmResolY = 1270;
	    }
	} else {
	    ErrorF("WACOM: unable to read resolution. Using default.\n");
	    common->wcmResolX = common->wcmResolY = 1270;
	}
    }

    DBG(2, ErrorF("serialGetResolution: ResolX=%d ResolY=%d\n",
                common->wcmResolX, common->wcmResolY));
}

static int serialGetRanges(WacomCommonPtr common, int fd)
{
    char buffer[BUFFER_SIZE];

    if (!(common->wcmMaxX && common->wcmMaxY)) {
	DBG(2, ErrorF("Requesting max coordinates\n"));
	if (!xf86WcmSendRequest(fd, WC_COORD, buffer, sizeof(buffer))) {
	    ErrorF("WACOM: unable to read max coordinates. "
                                "Use the MaxX and MaxY options.\n");
	    return !Success;
	}
	    DBG(2, ErrorF("%s\n", buffer));
	if (sscanf(buffer+2, "%d,%d", &common->wcmMaxX, &common->wcmMaxY) != 2) {
	    ErrorF("WACOM: unable to parse max coordinates. "
                                "Use the MaxX and MaxY options.\n");
	    return !Success;
	}
    }

    DBG(2, ErrorF("serialGetRanges: maxX=%d maxY=%d (%g,%g in)\n",
                common->wcmMaxX, common->wcmMaxY,
                (double)common->wcmMaxX / common->wcmResolX,
                (double)common->wcmMaxY / common->wcmResolY));

    return Success;
}

static int serialResetIntuos(WacomCommonPtr common, int fd)
{
    int err;
    SYSCALL(err = xf86WcmWrite(fd, intuos_setup_string, strlen(intuos_setup_string)));
    return (err == -1) ? !Success : Success;
}

static int serialResetCintiq(WacomCommonPtr common, int fd)
{
    int err;

    SYSCALL(err = xf86WcmWrite(fd, WC_RESET, strlen(WC_RESET)));

    if (xf86WcmWait(75)) return !Success;

    SYSCALL(err = xf86WcmWrite(fd, pl_setup_string, strlen(pl_setup_string)));
    if (err == -1) return !Success;

    SYSCALL(err = xf86WcmWrite(fd, penpartner_setup_string, 
		strlen(penpartner_setup_string)));

    return (err == -1) ? !Success : Success;
}

static int serialResetPenPartner(WacomCommonPtr common, int fd)
{
    int err;
    SYSCALL(err = xf86WcmWrite(fd, penpartner_setup_string,
                strlen(penpartner_setup_string)));
    return (err == -1) ? !Success : Success;
}

static int serialResetProtocol4(WacomCommonPtr common, int fd)
{
    int err;

    SYSCALL(err = xf86WcmWrite(fd, WC_RESET, strlen(WC_RESET)));

    if (xf86WcmWait(75)) return !Success;

    SYSCALL(err = xf86WcmWrite(fd, setup_string, strlen(setup_string)));
    if (err == -1) return !Success;

    SYSCALL(err = xf86WcmWrite(fd, penpartner_setup_string,
                strlen(penpartner_setup_string)));
    return (err == -1) ? !Success : Success;
}

static int serialEnableTiltProtocol4(WacomCommonPtr common, int fd)
{
    return Success;
}

static int serialEnableSuppressProtocol4(WacomCommonPtr common, int fd)
{
    char buf[20];
    int err;

    sprintf(buf, "%s%d\r", WC_SUPPRESS, common->wcmSuppress);
    SYSCALL(err = xf86WcmWrite(fd, buf, strlen(buf)));

    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }
    return Success;
}

static int serialSetLinkSpeedIntuos(WacomCommonPtr common, int fd)
{
    if ((common->wcmLinkSpeed == 38400) && (common->wcmVersion < 2.0F)) {
	ErrorF("Wacom: 38400 speed not supported with this Intuos "
                        "firmware (%f)\n", common->wcmVersion);
	ErrorF("Switching to 19200\n");
	common->wcmLinkSpeed = 19200;
    }
    return serialSetLinkSpeedProtocol5(common,fd);
}

static int serialSetLinkSpeedProtocol5(WacomCommonPtr common, int fd)
{
    int err;
    char* speed_init_string;

    DBG(1, ErrorF("Switching serial link to %d\n",
                common->wcmLinkSpeed));

    /* set init string according to speed */
    speed_init_string = (common->wcmLinkSpeed == 38400) ? WC_V_38400 : WC_V_19200;

    /* Switch the tablet to the requested speed */
    SYSCALL(err = xf86WcmWrite(fd, speed_init_string, strlen(speed_init_string)));

    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 75 mSecs */
    if (xf86WcmWait(75)) return !Success;

    /* Set speed of serial link to requested speed */
    if (xf86WcmSetSerialSpeed(fd, common->wcmLinkSpeed) < 0) return !Success;

    return Success;
}

static int serialStartTablet(WacomCommonPtr common, int fd)
{
    int err;

    /* Tell the tablet to start sending coordinates */
    SYSCALL(err = xf86WcmWrite(fd, WC_START, strlen(WC_START)));

    if (err == -1) {
	 ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    return Success;
}

static void serialParseP4Common(WacomCommonPtr common,
        const unsigned char* data, WacomDeviceState* last,
        WacomDeviceState* ds)
{
    int is_stylus = (data[0] & POINTER_BIT);
    int cur_type = is_stylus ? ((ds->buttons & 4) ? 
		ERASER_ID : STYLUS_ID) : CURSOR_ID;

    /* proximity bit */
    ds->proximity = (data[0] & PROXIMITY_BIT);

    /* x and y coordinates */
    ds->x = (((data[0] & 0x3) << 14) + (data[1] << 7) + data[2]);
    ds->y = (((data[3] & 0x3) << 14) + (data[4] << 7) + data[5]);

    /* first time into prox */
    if (!last->proximity && ds->proximity) ds->device_type = cur_type;

    /* out of prox */
    else if (!ds->proximity) memset(ds,0,sizeof(*ds));

    /* check on previous proximity */
    else if (is_stylus) {
	/* we were fooled by tip and second
	 * sideswitch when it came into prox */
	if ((ds->device_type != cur_type) && (ds->device_type == ERASER_ID)) {
	    /* send a prox-out for old device */
	    WacomDeviceState out = { 0 };
	    xf86WcmEvent(common,0,&out);
	    ds->device_type = cur_type;
	}
    }

    DBG(8, ErrorF("serialParseP4Common %s\n",
                ds->device_type == CURSOR_ID ? "CURSOR" :
                ds->device_type == ERASER_ID ? "ERASER " :
                ds->device_type == STYLUS_ID ? "STYLUS" : "NONE"));

    /* handle tilt values only for stylus */
    if (HANDLE_TILT(common) && is_stylus) {
	ds->tiltx = (data[7] & TILT_BITS);
	ds->tilty = (data[8] & TILT_BITS);
	if (data[7] & TILT_SIGN_BIT) ds->tiltx -= 64;
	if (data[8] & TILT_SIGN_BIT) ds->tilty -= 64;
    }
}

/*****************************************************************************
 * xf86WcmSerialValidate -- validates serial packet; returns 0 on success,
 *   positive number of bytes to skip on error.
 ****************************************************************************/

int xf86WcmSerialValidate(WacomCommonPtr common, const unsigned char* data)
{
    int i, bad = 0;

    /* check magic */
    for (i=0; i<common->wcmPktLength; ++i) {
	if ( ((i==0) && !(data[i] & HEADER_BIT)) || ((i!=0) && (data[i] & HEADER_BIT))) {
	    bad = 1;
	    DBG(6, ErrorF("xf86WcmSerialValidate: bad magic at %d "
			"v=%p l=%d\n", i, data, common->wcmPktLength));
	    if (i!=0 && (data[i] & HEADER_BIT)) return i;
	}
    }
    if (bad) return common->wcmPktLength;
    else return 0;
}

static WacomModel isdv4General =
{
    "General ISDV4",
    isdv4InitISDV4,
    NULL,                 /* resolution not queried */
    NULL,                 /* ranges not queried */
    NULL,                 /* reset not supported */
    NULL,                 /* tilt automatically enabled */
    NULL,                 /* suppress implemented in software */
    NULL,                 /* link speed unsupported */
    NULL,                 /* start not supported */
    isdv4Parse,
    xf86WcmFilterCoord,   /* input filtering */
};

/*****************************************************************************
 * isdv4Detect -- Test if the attached device is ISDV4.
 ****************************************************************************/

static Bool isdv4Detect(LocalDevicePtr local)
{
    WacomDevicePtr priv = (WacomDevicePtr) local->private;
    WacomCommonPtr common = priv->common;
    return (common->wcmForceDevice == DEVICE_ISDV4) ? 1 : 0;
}

/*****************************************************************************
 * isdv4Init --
 ****************************************************************************/

static Bool isdv4Init(LocalDevicePtr local)
{
    int err;
    WacomDevicePtr priv = (WacomDevicePtr)local->private;
    WacomCommonPtr common = priv->common;

    DBG(1, ErrorF("initializing ISDV4 tablet\n"));

    DBG(1, ErrorF("resetting tablet\n"));

    /* Set the speed of the serial link to 38400 */
    if (xf86WcmSetSerialSpeed(local->fd, 38400) < 0) return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));
    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 250 mSecs */
    if (xf86WcmWait(250)) return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET)));
    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 75 mSecs */
    if (xf86WcmWait(75)) return !Success;

    /* Set the speed of the serial link to 19200 */
    if (xf86WcmSetSerialSpeed(local->fd, 19200) < 0) return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));
    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 250 mSecs */
    if (xf86WcmWait(250)) return !Success;

    /* Send reset to the tablet */
    SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET)));
    if (err == -1) {
	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 75 mSecs */
    if (xf86WcmWait(75)) return !Success;

    xf86WcmFlushTablet(local->fd);

    DBG(2, ErrorF("not reading model -- Wacom TabletPC ISD V4\n"));
    return xf86WcmInitTablet(common,&isdv4General,local->fd,"unknown",0.0);
}

/*****************************************************************************
 * isdv4InitISDV4 -- Setup the device
 ****************************************************************************/

static void isdv4InitISDV4(WacomCommonPtr common, int fd,
        const char* id, float version)
{
    DBG(2, ErrorF("initializing as ISDV4 model\n"));

    /* set parameters */
    common->wcmProtocolLevel = 0;
    common->wcmMaxZ = 255;          /* max Z value (pressure)*/
    common->wcmResolX = 2570;       /* X resolution in points/inch */
    common->wcmResolY = 2570;       /* Y resolution in points/inch */
    common->wcmPktLength = 9;       /* length of a packet */

    if (common->wcmRotate==ROTATE_NONE) {
	common->wcmMaxX = 21136;
	common->wcmMaxY = 15900;
    } else if (common->wcmRotate==ROTATE_CW || common->wcmRotate==ROTATE_CCW) {
	common->wcmMaxX = 15900;
	common->wcmMaxY = 21136;
    }
}

static int isdv4Parse(WacomCommonPtr common, const unsigned char* data)
{
    WacomDeviceState* last = &common->wcmChannel[0].valid.state;
    WacomDeviceState* ds;
    int n, cur_type, tmp_coord;

    if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

    /* pick up where we left off, minus relative values */
    ds = &common->wcmChannel[0].work;
    RESET_RELATIVE(*ds);

    ds->proximity = (data[0] & 0x20);

    /* x and y in "normal" orientetion (wide length is X) */
    ds->x = (((int)data[6] & 0x60) >> 5) | ((int)data[2] << 2) |
                ((int)data[1] << 9);
    ds->y = (((int)data[6] & 0x18) >> 3) | ((int)data[4] << 2) |
                ((int)data[3] << 9);

    /* rotation mixes x and y up a bit */
    if (common->wcmRotate == ROTATE_CW) {
	tmp_coord = ds->x;
	ds->x = ds->y;
	ds->y = common->wcmMaxY - tmp_coord;
    } else if (common->wcmRotate == ROTATE_CCW) {
	tmp_coord = ds->y;
 	ds->y = ds->x;
	ds->x = common->wcmMaxX - tmp_coord;
    }

    /* pressure */
    ds->pressure = ((data[6] & 0x01) << 7) | (data[5] & 0x7F);

    /* button order is slightly amiss so that styluses with only
     * one side button have buttons 1 and 3 */

    /* report touch as button 1 */
    ds->buttons = (data[0] & 0x01) ? 1 : 0 ;

    /* report side switch as button 3 */
    ds->buttons |= (data[0] & 0x02) ? 0x04 : 0 ;

    /* report as button 2 (subject eraser check below) */
    ds->buttons |= (data[0] & 0x04) ? 0x02 : 0 ;

    /* the bit used for button 2 is also used to ID the
     * eraser.  If the tool comes into proximity with this bit
     * set, it is assumed to be the eraser.  If the tool was
     * previously in proximity, it is assumed to be the button.
     * The mistaken identity case is handled with proximity below. */

    /* check which device we have */
    cur_type = (data[0] & 0x04) ? ERASER_ID : STYLUS_ID;

    /* If there's no eraser configured, let's not even try to
     * report an eraser (handy for making button 2 action more
     * definite) */
    if ( common->wcmNumDevices == 1 && cur_type == ERASER_ID ) cur_type = STYLUS_ID;

    /* first time into prox */
    if (!last->proximity && ds->proximity) ds->device_type = cur_type;

    /* out of prox */
    else if (!ds->proximity) memset(ds,0,sizeof(*ds));

    /* check on previous proximity */
    else {
	/* we were fooled by tip and second
	 * sideswitch when it came into prox */
	if ((ds->device_type != cur_type) && (ds->device_type == ERASER_ID)) {
	    /* send a prox-out for old device */
	    WacomDeviceState out = { 0 };
	    xf86WcmEvent(common,0,&out);
	    ds->device_type = cur_type;
	}
    }

    DBG(8, ErrorF("isdv4Parse %s\n", ds->device_type == ERASER_ID ? "ERASER " :
                ds->device_type == STYLUS_ID ? "STYLUS" : "NONE"));

    xf86WcmEvent(common,0,ds);

    return common->wcmPktLength;
}

static void filterNearestPoint(double x0, double y0, double x1, double y1,
                double a, double b, double* x, double* y)
{
    double vx, vy, wx, wy, d1, d2, c;

    wx = a - x0; wy = b - y0;
    vx = x1 - x0; vy = y1 - y0;

    d1 = vx * wx + vy * wy;
    if (d1 <= 0) {
	*x = x0;
	*y = y0;
    } else {
	d2 = vx * vx + vy * vy;
	if (d1 >= d2) {
	    *x = x1;
	    *y = y1;
	} else {
	    c = d1 / d2;
	    *x = x0 + c * vx;
	    *y = y0 + c * vy;
	}
    }
}

static int filterOnLine(double x0, double y0, double x1, double y1,
                double a, double b)
{
    double x, y, d;
    filterNearestPoint(x0,y0,x1,y1,a,b,&x,&y);
    d = (x-a)*(x-a) + (y-b)*(y-b);
    return d < 0.00001; /* within 100th of a point (1E-2 squared) */
}

static void filterLine(int* pCurve, int nMax, int x0, int y0, int x1, int y1)
{
    int dx, dy, ax, ay, sx, sy, x, y, d;

    /* sanity check */
    if ((x0 < 0) || (y0 < 0) || (x1 < 0) || (y1 < 0) ||
                (x0 > nMax) || (y0 > nMax) || (x1 > nMax) || (y1 > nMax))
	return;

    dx = x1 - x0; ax = abs(dx) * 2; sx = (dx>0) ? 1 : -1;
    dy = y1 - y0; ay = abs(dy) * 2; sy = (dy>0) ? 1 : -1;
    x = x0; y = y0;

    /* x dominant */
    if (ax > ay) {
	d = ay - ax / 2;
	while (1) {
	    pCurve[x] = y;
	    if (x == x1) break;
	    if (d >= 0) {
		y += sy;
		d -= ax;
	    }
	    x += sx;
	    d += ay;
	}
    } else {		/* y dominant */
	d = ax - ay / 2;
	while (1) {
	    pCurve[x] = y;
	    if (y == y1) break;
	    if (d >= 0) {
		x += sx;
		d -= ay;
	    }
	    y += sy;
	    d += ax;
	}
    }
}

static void filterCurveToLine(int* pCurve, int nMax, double x0, double y0,
         double x1, double y1, double x2, double y2, double x3, double y3)
{
    double x01,y01,x32,y32,xm,ym;
    double c1,d1,c2,d2,e,f;

    /* check if control points are on line */
    if (filterOnLine(x0,y0,x3,y3,x1,y1) && filterOnLine(x0,y0,x3,y3,x2,y2)) {
	filterLine(pCurve,nMax,(int)(x0*nMax),(int)(y0*nMax),
                        (int)(x3*nMax),(int)(y3*nMax));
	return;
    }

    /* calculate midpoints */
    x01 = (x0 + x1) / 2; y01 = (y0 + y1) / 2;
    x32 = (x3 + x2) / 2; y32 = (y3 + y2) / 2;

    /* calc split point */
    xm = (x1 + x2) / 2; ym = (y1 + y2) / 2;

    /* calc control points and midpoint */
    c1 = (x01 + xm) / 2; d1 = (y01 + ym) / 2;
    c2 = (x32 + xm) / 2; d2 = (y32 + ym) / 2;
    e = (c1 + c2) / 2; f = (d1 + d2) / 2;

    /* do each side */
    filterCurveToLine(pCurve,nMax,x0,y0,x01,y01,c1,d1,e,f);
    filterCurveToLine(pCurve,nMax,e,f,c2,d2,x32,y32,x3,y3);
}

static void filterIntuosCoord(int* coord, int* current, int tilt, int* state)
{
    int ts;
    int x0_pred;
    int x0_pred1;
    int x0, x1, x2, x3;

    x0 = *current;
    x1 = coord[0];
    x2 = coord[1];
    x3 = coord[2];
    coord[0] = x0;
    coord[1] = x1;
    coord[2] = x2;

    ts = tilt >= 0 ? 1 : -1;

    if (*state == 0 || *state == 3) {
	if (ts * (x0 - 2 * x1 - x2) > 12 && ts * (x0 - 3 * x2 - 2 * x3) > 12) {
	    /* detected a jump at x0 */
	    *state = 1;
	    *current = x1;
	} else if (*state == 0) {
	    x0_pred = 7 * x0 + 14 * x1 + 15 * x2 + 16;
	    x0_pred1 = 4 * x3;
	    if (x0_pred > x0_pred1) *current = ((CARD32)(x0_pred - x0_pred1)) >> 5;
	    else *current = 0;
	} else {
	    /* state == 3
	     * a jump at x3 was detected */
	    *current = (x0 + 2 * x1 + x2 + 2) >> 2;
	    *state = 0;
 	}
    } else if (*state == 1) {
	/* a jump at x1 was detected */
	x0_pred = 3 * x0 + 7 * x2 + 4;
	x0_pred1 = 2 * x3;
	if (x0_pred > x0_pred1)
 	    *current = ((CARD32)(x0_pred - x0_pred1)) >> 3;
	else
	    *current = 0;
	*state = 2;
    } else {
	/* state == 2
	 * a jump at x2 was detected */
	*current = x1;
	*state = 3;
    }
}

/*****************************************************************************
 * filterIntuosTilt --
 *   Correct some hardware defects we've been seeing in Intuos pads,
 *   but also cuts down quite a bit on jitter.
 ****************************************************************************/

static void filterIntuosTilt(int* state, int* tilt)
{
    int tx;

    tx = *tilt + state[0] + state[1] + state[2];

    state[2] = state[1];
    state[1] = state[0];
    state[0] = *tilt;

    tx /= MAX_SAMPLES;

    if (tx > 63) tx = 63;
    else if (tx < -64) tx = -64;

    *tilt = tx;
}

/*****************************************************************************
 * filterIntuosStylus --
 *   Correct some hardware defects we've been seeing in Intuos pads,
 *   but also cuts down quite a bit on jitter.
 ****************************************************************************/

static void filterIntuosStylus(WacomFilterStatePtr state, WacomDeviceStatePtr ds)
{
    if (!state->npoints) {
	++state->npoints;
	DBG(2,ErrorF("filterIntuosStylus: first sample NO_FILTER\n"));
	state->x[0] = state->x[1] = state->x[2] = ds->x;
	state->y[0] = state->y[1] = state->y[2] = ds->y;
	state->tiltx[0] = state->tiltx[1] = state->tiltx[2] = ds->tiltx;
	state->tilty[0] = state->tilty[1] = state->tilty[2] = ds->tilty;
	return;
    }

    /* filter x */
    filterIntuosCoord(state->x, &ds->x, ds->tiltx, &state->statex);
    /* filter y */
    filterIntuosCoord(state->y, &ds->y, ds->tilty, &state->statey);
    /* filter tiltx */
    filterIntuosTilt(state->tiltx, &ds->tiltx);
    /* filter tilty */
    filterIntuosTilt(state->tilty, &ds->tilty);
}

/*****************************************************************************
 * xf86WcmSetPressureCurve -- apply user-defined curve to pressure values
 ****************************************************************************/

static void xf86WcmSetPressureCurve(WacomDevicePtr pDev, int x0, int y0, int x1, int y1)
{
    int i;

    /* sanity check values */
    if ((x0 < 0) || (x0 > 100) || (y0 < 0) || (y0 > 100) ||
                (x1 < 0) || (x1 > 100) || (y1 < 0) || (y1 > 100)) return;

    xf86Msg(X_INFO, "xf86WcmSetPressureCurve: setting to %d,%d %d,%d\n",
                x0, y0, x1, y1);

    /* if curve is not allocated, do it now. */
    if (!pDev->pPressCurve) {
	pDev->pPressCurve = (int*) xalloc(sizeof(int) * (FILTER_PRESSURE_RES + 1));
	if (!pDev->pPressCurve) {
	    xf86Msg(X_ERROR, "xf86WcmSetPressureCurve: failed to "
                                "allocate memory for curve\n");
	    return;
	}
    }

    /* linear by default */
    for (i=0; i<=FILTER_PRESSURE_RES; ++i) pDev->pPressCurve[i] = i;

    /* draw bezier line from bottom-left to top-right using ctrl points */
    filterCurveToLine(pDev->pPressCurve, FILTER_PRESSURE_RES,
                0.0, 0.0,               /* bottom left  */
                x0/100.0, y0/100.0,     /* control point 1 */
                x1/100.0, y1/100.0,     /* control point 2 */
                1.0, 1.0);              /* top right */

    for (i=0; i<=FILTER_PRESSURE_RES; i+=128)
	DBG(6, ErrorF("PRESSCURVE: %d=%d (%d)\n",i,pDev->pPressCurve[i],
                        pDev->pPressCurve[i] - i));

    pDev->nPressCtrl[0] = x0;
    pDev->nPressCtrl[1] = y0;
    pDev->nPressCtrl[2] = x1;
    pDev->nPressCtrl[3] = y1;
}

/*****************************************************************************
 * xf86WcmFilterIntuos -- provide error correction to all transducers except Intuos
 ****************************************************************************/

static int xf86WcmFilterCoord(WacomCommonPtr common, WacomChannelPtr pChannel,
        WacomDeviceStatePtr ds)
{
    /* Only noise correction should happen here. If there's a problem that
     * cannot be fixed, return 1 such that the data is discarded. */

    WacomDeviceState* pLast;
    int *x, *y;
    int filter_x, filter_y;

    x = pChannel->rawFilter.x;
    y = pChannel->rawFilter.y;
    if (!pChannel->rawFilter.npoints) {
	++pChannel->rawFilter.npoints;
	DBG(2,ErrorF("xf86WcmFilterCoord: first sample NO_FILTER\n"));
	x[0] = x[1] = x[2] = ds->x;
	y[0] = y[1] = y[2] = ds->y;
	return 0;
    }

    pLast = &pChannel->valid.state;
    filter_x = (ds->x + x[0] + x[1] + x[2])/4;
    filter_y = (ds->y + y[0] + y[1] + y[2])/4;

    x[2] = x[1];
    y[2] = y[1];
    x[1] = x[0];
    y[1] = y[0];
    x[0] = ds->x;
    y[0] = ds->y;

    if (abs(filter_x - pLast->x) > 4)
	ds->x = filter_x;
    else
	ds->x = pLast->x;

    if (abs(filter_y - pLast->y) > 4)
	ds->y = filter_y;
    else
	ds->y = pLast->y;

    return 0; /* lookin' good */
}

/*****************************************************************************
 * xf86WcmFilterCoord -- provide error correction to Intuos and Intuos2
 ****************************************************************************/

static int xf86WcmFilterIntuos(WacomCommonPtr common, WacomChannelPtr pChannel,
        WacomDeviceStatePtr ds)
{
    /* Only error correction should happen here. If there's a problem that
     * cannot be fixed, return 1 such that the data is discarded. */

    if (ds->device_type != CURSOR_ID)
	filterIntuosStylus(&pChannel->rawFilter, ds);
    else
	xf86WcmFilterCoord(common, pChannel, ds);

    return 0; /* lookin' good */
}



/*
 ***************************************************************************
 *
 * xf86WcmDevConvert --
 *	Convert valuators to X and Y. Only used by core events.
 *
 ***************************************************************************
 */
static Bool
xf86WcmDevConvert(LocalDevicePtr	local,
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
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;
    
    DBG(6, ErrorF("xf86WcmDevConvert\n"));

    if (first != 0 || num == 1)
      return FALSE;
   
#ifdef PANORAMIX
    if (!noPanoramiXExtension && (priv->flags & ABSOLUTE_FLAG) &&
                priv->common->wcmGimp) {
	int i, totalWidth, leftPadding = 0;
	for (i = 0; i < priv->currentScreen; i++)
	    leftPadding += screenInfo.screens[i]->width;
	    for (totalWidth = leftPadding; i < priv->numScreen; i++)
		totalWidth += screenInfo.screens[i]->width;
	    v0 -= (priv->bottomX - priv->topX) * leftPadding
                        / (double)totalWidth + 0.5;
    }
#endif
    if (priv->twinview != TV_NONE && (priv->flags & ABSOLUTE_FLAG)) {
	v0 -= priv->topX;
	v1 -= priv->topY;
	if (priv->twinview == TV_LEFT_RIGHT) {
	    if (v0 > priv->bottomX)
	    {
		v0 -= priv->bottomX;
		priv->currentScreen = 1;
		if (priv->screen_no == 0) {
		    priv->currentScreen = 0;
		}
	    } else {
		priv->currentScreen = 0;
		if (priv->screen_no == 1) {
		    priv->currentScreen = 1;
		}
	    }
	    if (priv->currentScreen == 1) {
		*x = priv->tvResolution[0] + priv->tvResolution[2]
                               * v0 / (priv->bottomX - priv->topX);
		*y = v1 * priv->tvResolution[3] /
                               (priv->bottomY - priv->topY) + 0.5;
	    } else {
		*x = priv->tvResolution[0] * v0 /
                               (priv->bottomX - priv->topX);
		*y = v1 * priv->tvResolution[1] /
                               (priv->bottomY - priv->topY) + 0.5;
	    }
 	}
	if (priv->twinview == TV_ABOVE_BELOW) {
	    if (v1 > priv->bottomY) {
		v1 -= priv->bottomY;
		priv->currentScreen = 1;
		if (priv->screen_no == 0) {
		    priv->currentScreen = 0;
		}
	    } else {
		priv->currentScreen = 0;
		if (priv->screen_no == 1) {
		    priv->currentScreen = 1;
		}
	    }
	    if (priv->currentScreen == 1) {
		*x = v0 * priv->tvResolution[2] /
                                   (priv->bottomX - priv->topX) + 0.5;
		*y = priv->tvResolution[1] + priv->tvResolution[3] * v1
                                   / (priv->bottomY - priv->topY);
	    } else {
		*x = v0 * priv->tvResolution[0] /
                                  (priv->bottomX - priv->topX) + 0.5;
		*y = priv->tvResolution[1] * v1 /
                                   (priv->bottomY - priv->topY);
	    }
	}
    } else {
	*x = v0 * priv->factorX + 0.5;
	*y = v1 * priv->factorY + 0.5;
    }
    DBG(6, ErrorF("Wacom converted v0=%d v1=%d to x=%d y=%d\n",
		  v0, v1, *x, *y));
    return TRUE;
}

/*
 ***************************************************************************
 *
 * xf86WcmDevReverseConvert --
 *	Convert X and Y to valuators. Only used by core events.
 *
 ***************************************************************************
 */
static Bool
xf86WcmDevReverseConvert(LocalDevicePtr	local,
		      int		x,
		      int		y,
		      int		*valuators)
{
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;
    
    valuators[0] = x / priv->factorX + 0.5;
    valuators[1] = y / priv->factorY + 0.5;

#ifdef PANORAMIX
    if (!noPanoramiXExtension && (priv->flags & ABSOLUTE_FLAG) &&
                priv->common->wcmGimp) {
	int i, totalWidth, leftPadding = 0;
	for (i = 0; i < priv->currentScreen; i++)
	    leftPadding += screenInfo.screens[i]->width;
	for (totalWidth = leftPadding; i < priv->numScreen; i++)
	    totalWidth += screenInfo.screens[i]->width;
	valuators[0] += (priv->bottomX - priv->topX)
                        * leftPadding / (double)totalWidth + 0.5;
    }
#endif
    if (priv->twinview != TV_NONE && (priv->flags & ABSOLUTE_FLAG)) {
 	if (priv->twinview == TV_LEFT_RIGHT) {
	    if (x > priv->tvResolution[0]) {
		x -= priv->tvResolution[0];
		priv->currentScreen = 1;
		if (priv->screen_no == 0) {
		    priv->currentScreen = 0;
		}
	    } else {
		priv->currentScreen = 0;
		if (priv->screen_no == 1) {
		    priv->currentScreen = 1;
		}
	    }
	    if (priv->currentScreen == 1) {
		valuators[0] = x * (priv->bottomX - priv->topX)
			/ priv->tvResolution[2] + priv->bottomX - priv->topX + 0.5;
		valuators[1] = y * (priv->bottomY - priv->topY) /
			 priv->tvResolution[3] + 0.5;
	    } else {
		valuators[0] = x * (priv->bottomX - priv->topX)
			/ priv->tvResolution[0] + 0.5;
		valuators[1] = y * (priv->bottomY - priv->topY) /
			 priv->tvResolution[1] + 0.5;
	    }
	}
	if (priv->twinview == TV_ABOVE_BELOW) {
	    if (y > priv->tvResolution[1]) {
		y -= priv->tvResolution[1];
		priv->currentScreen = 1;
		if (priv->screen_no == 0) {
		    priv->currentScreen = 0;
		}
	    } else {
		priv->currentScreen = 0;
		if (priv->screen_no == 1) {
		    priv->currentScreen = 1;
		}
	    }
	    if (priv->currentScreen == 1) {
		valuators[0] = x * (priv->bottomX - priv->topX) /
			priv->tvResolution[2] + 0.5;
		valuators[1] = y *(priv->bottomY - priv->topY) / 
 			priv->tvResolution[3] + priv->bottomY - priv->topY + 0.5;
	    } else {
		valuators[0] = x * (priv->bottomX - priv->topX) /
			priv->tvResolution[0] + 0.5;
		valuators[1] = y *(priv->bottomY - priv->topY) / 
			priv->tvResolution[1] + 0.5;
	    }
	}
	valuators[0] += priv->topX;
	valuators[1] += priv->topY;
    }
    DBG(6, ErrorF("Wacom converted x=%d y=%d to v0=%d v1=%d\n", x, y,
		  valuators[0], valuators[1]));

    return TRUE;
}
 
/*
 ***************************************************************************
 *
 * xf86WcmSetScreen --
 *      set to the proper screen according to the converted (x,y).
 *      this only supports for horizontal setup now.
 *      need to know screen's origin (x,y) to support 
 *      combined horizontal and vertical setups
 *
 ***************************************************************************
 */
static void
xf86WcmSetScreen(LocalDevicePtr   local,
               int          *v0,
               int          *v1)
{
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;
    int                 screenToSet = miPointerCurrentScreen()->myNum;
    int                 i;
    int 		x, y;
    int 		totalWidth = 0, maxHeight = 0, leftPadding = 0;
    double 		sizeX = priv->bottomX - priv->topX;
    double 		sizeY = priv->bottomY - priv->topY;

    /* set factorX and factorY for single screen setup since
     * Top X Y and Bottom X Y can be changed while driver is running
     */
    if (screenInfo.numScreens == 1) {
	priv->factorX = screenInfo.screens[0]->width / sizeX;
	priv->factorY = screenInfo.screens[0]->height / sizeY;
	return;
    }

    if (!(local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER))) return;
    if (!(priv->flags & ABSOLUTE_FLAG)) {
	/* screenToSet lags by one event, but not that important */
	screenToSet = miPointerCurrentScreen()->myNum;
	priv->factorX = screenInfo.screens[screenToSet]->width / sizeX;
	priv->factorY = screenInfo.screens[screenToSet]->height / sizeY;
	priv->currentScreen = screenToSet;
	return;
    }

    for (i = 0; i < priv->numScreen; i++) {
	totalWidth += screenInfo.screens[i]->width;
	if (maxHeight < screenInfo.screens[i]->height)
	    maxHeight = screenInfo.screens[i]->height;
    }

    if (priv->screen_no == -1) {
	for (i = 0; i < priv->numScreen; i++) {
	    if (*v0 * totalWidth <= (leftPadding + 
			screenInfo.screens[i]->width) * sizeX) {
		screenToSet = i;
		break;
	    }
	    leftPadding += screenInfo.screens[i]->width;
	}
    }
#ifdef PANORAMIX
    else if (!noPanoramiXExtension && priv->common->wcmGimp) {
	screenToSet = priv->screen_no;
	for (i = 0; i < screenToSet; i++)
	    leftPadding += screenInfo.screens[i]->width;
	*v0 = (sizeX * leftPadding + *v0 * screenInfo.screens[screenToSet]->width) /
			(double)totalWidth + 0.5;
	*v1 = *v1 * screenInfo.screens[screenToSet]->height / (double)maxHeight + 0.5;
    }

    if (!noPanoramiXExtension && priv->common->wcmGimp) {
	priv->factorX = totalWidth/sizeX;
	priv->factorY = maxHeight/sizeY;
	x = (*v0 - sizeX * leftPadding / totalWidth) * priv->factorX + 0.5;
	y = *v1 * priv->factorY + 0.5;

	if (x >= screenInfo.screens[screenToSet]->width)
 	    x = screenInfo.screens[screenToSet]->width - 1;
	if (y >= screenInfo.screens[screenToSet]->height)
	    y = screenInfo.screens[screenToSet]->height - 1;
    }
    else
#endif
    {
	if (priv->screen_no == -1)
	    *v0 = (*v0 * totalWidth - sizeX * leftPadding)
			/ screenInfo.screens[screenToSet]->width;
	else
	    screenToSet = priv->screen_no;
	priv->factorX = screenInfo.screens[screenToSet]->width / sizeX;
	priv->factorY = screenInfo.screens[screenToSet]->height / sizeY;

	x = *v0 * priv->factorX + 0.5;
	y = *v1 * priv->factorY + 0.5;
    }

    xf86XInputSetScreen(local, screenToSet, x, y);
    DBG(10, ErrorF("xf86WcmSetScreen current=%d ToSet=%d\n",
		priv->currentScreen, screenToSet));
    priv->currentScreen = screenToSet;
}
 
/*
 ***************************************************************************
 *
 * xf86WcmSendButtons --
 *	Send button events by comparing the current button mask with the
 *      previous one.
 *
 ***************************************************************************
 */
static void
xf86WcmSendButtons(LocalDevicePtr	local,
		   int                  buttons,
		   int                  rx,
		   int                  ry,
		   int                  rz,
		   int                  rtx,
		   int                  rty,
		   int                  rrot,
		   int                  rth,
		   int			rwheel)
		   
{
    int             button, newb;
    WacomDevicePtr  priv = (WacomDevicePtr) local->private;

    for (button=1; button<=16; button++) {
	int mask = 1 << (button-1);
	
	if ((mask & priv->oldButtons) != (mask & buttons)) {
	    DBG(4, ErrorF("xf86WcmSendButtons button=%d state=%d, for %s\n", 
			  button, (buttons & mask) != 0, local->name));
	    /* set to the configured buttons */
	    newb = button;
	    if (priv->button[button-1] != button)
		newb = priv->button[button-1];

	    /* translate into Left Double Click */
	    if (newb == 17)
	    {
		newb = 1;
		if (buttons & mask)
		{
		    /* Left button down */
		    if (IsCursor(priv))
			xf86PostButtonEvent(local->dev,
				(priv->flags & ABSOLUTE_FLAG), newb, 1,
				0, 6, rx, ry, rz, rrot, rth, rwheel);
		    else
			xf86PostButtonEvent(local->dev,
				(priv->flags & ABSOLUTE_FLAG), newb, 1,
				0, 6, rx, ry, rz, rtx, rty, rwheel);
		    /* Left button up */
		    if (IsCursor(priv))
			xf86PostButtonEvent(local->dev, (priv->flags & ABSOLUTE_FLAG), 
				newb, 0, 0, 6, rx, ry, rz, rrot, rth, rwheel);
		    else
			xf86PostButtonEvent(local->dev, (priv->flags & ABSOLUTE_FLAG), 
				newb, 0, 0, 6, rx, ry, rz, rtx, rty, rwheel);
		}
	    }
	    if (newb <= 17)
	    {
		if (IsCursor(priv))
		    xf86PostButtonEvent(local->dev, (priv->flags & ABSOLUTE_FLAG),
				newb, (buttons & mask) != 0,
				0, 6, rx, ry, rz, rrot, rth, rwheel);
		else
		    xf86PostButtonEvent(local->dev, (priv->flags & ABSOLUTE_FLAG),
				newb, (buttons & mask) != 0,
				0, 6, rx, ry, rz, rtx, rty, rwheel);
	    }
	}
    }
}

/*
 ***************************************************************************
 *
 * xf86WcmSendEvents --
 *	Send events according to the device state.
 *
 ***************************************************************************
 */
static void
xf86WcmSendEvents(LocalDevicePtr	local,
		  const WacomDeviceState *ds)
{
    int 		type = ds->device_type;
    int 		is_button = !!(ds->buttons);
    int 		is_proximity = ds->proximity;
    int 		x = ds->x;
    int 		y = ds->y;
    int 		z = ds->pressure;
    int 		buttons = ds->buttons;
    int 		tx = ds->tiltx;
    int 		ty = ds->tilty;
    int 		rot = ds->rotation;
    int 		throttle = ds->throttle;
    int 		wheel = ds->abswheel;
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;
    WacomCommonPtr	common = priv->common;
    int			rx, ry, rz, rtx, rty, rrot, rth, rw;
    int			is_core_pointer, is_absolute;
    int			aboveBelowSwitch = (priv->twinview == TV_ABOVE_BELOW) ? 
 				((y < priv->topY) ? -1 : ((priv->bottomY < y) ? 1 : 0)) : 0;
    int			leftRightSwitch = (priv->twinview == TV_LEFT_RIGHT) ? 
				((x < priv->topX) ? -1 : ((priv->bottomX < x) ? 1 : 0)) : 0;

    DBG(7, ErrorF("[%s] prox=%s\tx=%d\ty=%d\tz=%d\tbutton=%s\tbuttons=%d\ttx=%d ty=%d\twl=%d rot=%d th=%d\n",
		  (type == STYLUS_ID) ? "stylus" : (type == CURSOR_ID) ? "cursor" : "eraser",
		  is_proximity ? "true" : "false",
		  x, y, z,
		  is_button ? "true" : "false", buttons,
		  tx, ty, wheel, rot, throttle));

    is_absolute = (priv->flags & ABSOLUTE_FLAG);
    is_core_pointer = xf86IsCorePointer(local->dev);

    if ( is_proximity || x || y || z || buttons || tx || ty || wheel )
    {
	switch ( leftRightSwitch )
	{
	    case -1:
		priv->doffsetX = 0;
		break;
	    case 1:
		priv->doffsetX = priv->bottomX - priv->topX;
		break;
	}
	switch ( aboveBelowSwitch )
	{
	    case -1:
		priv->doffsetY = 0;
		break;
	    case 1:
		priv->doffsetY = priv->bottomY - priv->topY;
		break;
	}
    }

    x += priv->doffsetX;
    y += priv->doffsetY;

    /* sets rx and ry according to the mode */
    if (is_absolute) {
	if (priv->twinview == TV_NONE)
	{
	    rx = x > priv->bottomX ? priv->bottomX - priv->topX : 
			x < priv->topX ? 0 : x - priv->topX;
	    ry = y > priv->bottomY ? priv->bottomY - priv->topY : 
			y < priv->topY ? 0 : y - priv->topY;
	}
	else
	{
	    rx = x;
	    ry = y;
	}
	rz = z;
	rtx = tx;
	rty = ty;
	rrot = rot;
	rth = throttle;
	rw = wheel;
    } else {
	if (priv->oldProximity)
	{
	    /* unify acceleration in both directions */
	    rx = (x - priv->oldX) * priv->factorY / priv->factorX;
	    ry = y - priv->oldY;
	}
	else
	{
	    rx = 0;
	    ry = 0;
	}
	if (priv->speed != DEFAULT_SPEED ) {
            /* don't apply acceleration for fairly small
             * increments (but larger than speed setting).
             */
            int no_jitter = priv->speed * 3;
	    double param = priv->speed;
	    double relacc = (MAX_ACCEL-priv->accel)*(MAX_ACCEL-priv->accel);
            if (ABS(rx) > no_jitter)
	    {
		/* don't apply acceleration when too fast. */
		param += priv->accel > 0 ? rx/relacc : 0;
		if (param > 20.00)
		    rx *= param;
	    }
            if (ABS(ry) > no_jitter)
	    {
		param += priv->accel > 0 ? ry/relacc : 0;
		if (param > 20.00)
		    ry *= param;
	    }
        }
	rz = z - priv->oldZ;
	rtx = tx - priv->oldTiltX;
	rty = ty - priv->oldTiltY;
	rrot = rot - priv->oldRot;
	rth = throttle - priv->oldThrottle;
	rw = wheel - priv->oldWheel;
    }

    /* for multiple monitor support, we need to set the proper
     * screen and modify the axes before posting events */
    xf86WcmSetScreen(local, &rx, &ry);

    /* coordinates are ready we can send events */
    if (is_proximity) {

	if (!priv->oldProximity) {
	    if (IsCursor(priv))
		xf86PostProximityEvent(local->dev, 1, 0, 6, rx, ry, rz, rrot, rth, rw);
	    else
		xf86PostProximityEvent(local->dev, 1, 0, 6, rx, ry, rz, rtx, rty, rw);
	}

	if(!(priv->flags & BUTTONS_ONLY_FLAG)) {
	    if (IsCursor(priv))
		xf86PostMotionEvent(local->dev, is_absolute, 0, 6, rx, ry, rz, rrot, rth, rw);
	    else
		xf86PostMotionEvent(local->dev, is_absolute, 0, 6, rx, ry, rz, rtx, rty, rw);
	}
	/* simulate button 4 and 5 for relative wheel */
	if ( ds->relwheel )
	{
	    int fakeButton = ds->relwheel > 0 ? 5 : 4;
	    int i;
	    for (i=0; i<abs(ds->relwheel); i++)
	    {
		xf86PostButtonEvent(local->dev, is_absolute, fakeButton, 
 			1, 0, 6, rx, ry, rz, rrot, rth, rw);
		xf86PostButtonEvent(local->dev, is_absolute, fakeButton, 
			0, 0, 6, rx, ry, rz, rrot, rth, rw);
	    }
	}

	if (priv->oldButtons != buttons) {
	    xf86WcmSendButtons (local, buttons, rx, ry, rz, rtx, rty, rrot, rth, rw);
	}
    }
    else { /* !PROXIMITY */
	/* reports button up when the device has been down and becomes out of proximity */
	if (priv->oldButtons) {
	    buttons = 0;
	    xf86WcmSendButtons (local, buttons, rx, ry, rz, rtx, rty, rrot, rth, rw);
	}
	if (!is_core_pointer) {
	    /* macro button management */
	    if (common->wcmProtocolLevel == 4 && buttons) {
		int	macro = z / 2;

		DBG(6, ErrorF("macro=%d buttons=%d wacom_map[%d]=%lx\n",
			      macro, buttons, macro, (unsigned long)wacom_map[macro]));

		/* First available Keycode begins at 8 => macro+7 */
		/* key down */
		if (IsCursor(priv))
		    xf86PostKeyEvent(local->dev,macro+7,1,is_absolute,
				0,6,0,0,buttons,rrot,rth,rw);
		else
		    xf86PostKeyEvent(local->dev,macro+7,1,is_absolute,
				0,6,0,0,buttons,rtx,rty,rw);

		/* key up */
		if (IsCursor(priv))
		    xf86PostKeyEvent(local->dev,macro+7,0,is_absolute,
				0,6,0,0,buttons,rrot,rth,rw);
		else
		    xf86PostKeyEvent(local->dev,macro+7,0,is_absolute,
				0,6,0,0,buttons,rtx,rty,rw);

	    }
	}
	if (priv->oldProximity)
	{
	    if (IsCursor(priv))
		xf86PostProximityEvent(local->dev, 0, 0, 6, 
				rx, ry, rz, rrot, rth, rw);
	    else
		xf86PostProximityEvent(local->dev,0, 0, 6, 
				rx, ry, rz, rtx, rty, rw);
	}
    }

    priv->oldProximity = is_proximity;
    priv->oldButtons = buttons;
    priv->oldWheel = wheel;
    priv->oldX = x;
    priv->oldY = y;
    priv->oldZ = z;
    priv->oldTiltX = tx;
    priv->oldTiltY = ty;
    priv->oldRot = rot;
    priv->oldThrottle = throttle;
}

/*
 ***************************************************************************
 *
 * xf86WcmSuppress --
 *	Determine whether device state has changed enough - return 1
 *	if not.
 *
 ***************************************************************************
 */
static int
xf86WcmSuppress(int			suppress,
		WacomDeviceState 	*ds1,
		WacomDeviceState	*ds2)
{
    if (ds1->buttons != ds2->buttons) return 0;
    if (ds1->proximity != ds2->proximity) return 0;
    if (ABS(ds1->x - ds2->x) > suppress) return 0;
    if (ABS(ds1->y - ds2->y) > suppress) return 0;
    if (ABS(ds1->pressure - ds2->pressure) > suppress) return 0;
    if (ABS(ds1->throttle - ds2->throttle) > suppress) return 0;
    if ((1800 + ds1->rotation - ds2->rotation) % 1800 > suppress &&
    	(1800 + ds2->rotation - ds1->rotation) % 1800 > suppress) return 0;
    if (ABS(ds1->abswheel - ds2->abswheel) > suppress ||
		(ds2->relwheel) != 0) return 0;
    return 1;
}

/*
 ***************************************************************************
 *
 * xf86WcmDevReadInput --
 *	Read the device on IO signal.
 *
 ***************************************************************************
 */
static void
xf86WcmDevReadInput(LocalDevicePtr local)
{
    int loop=0;
#   define MAX_READ_LOOPS 10

    WacomCommonPtr common = ((WacomDevicePtr)local->private)->common;

    /* move data until we exhaust the device */
    for (loop=0; loop < MAX_READ_LOOPS; ++loop)
    {
	/* dispatch */
	common->wcmDevCls->Read(local);

	/* verify that there is still data in pipe */
	if (!xf86WcmReady(local->fd)) break;
    }

    /* report how well we're doing */
    if (loop >= MAX_READ_LOOPS) {
	DBG(1,ErrorF("xf86WcmDevReadInput: Can't keep up!!!\n"));
    }
    else {
	if (loop > 1) DBG(10,ErrorF("xf86WcmDevReadInput: Read (%d)\n",loop));
    }
}

void xf86WcmReadPacket(LocalDevicePtr local)
{
    WacomCommonPtr common = ((WacomDevicePtr)(local->private))->common;
    int len, pos, cnt, remaining;

    if (!common->wcmModel) return;

    remaining = sizeof(common->buffer) - common->bufpos;

    DBG(7, ErrorF("xf86WcmDevReadPacket: device=%s fd=%d "
                "pos=%d remaining=%d\n",
                common->wcmDevice, local->fd,
                common->bufpos, remaining));

    /* fill buffer with as much data as we can handle */
    SYSCALL(len = xf86WcmRead(local->fd,
                common->buffer + common->bufpos, remaining));

    if (len <= 0)
    {
	ErrorF("Error reading wacom device : %s\n", strerror(errno));
 	return;
    }

    /* account for new data */
    common->bufpos += len;
    DBG(10, ErrorF("xf86WcmReadPacket buffer has %d bytes\n", common->bufpos));

    pos = 0;

    /* while there are whole packets present, parse data */
    while ((common->bufpos - pos) >=  common->wcmPktLength)
    {
	/* parse packet */
	cnt = common->wcmModel->Parse(common, common->buffer + pos);
	if (cnt <= 0)
	{
	    DBG(1,ErrorF("Misbehaving parser returned %d\n",cnt));
	    break;
	}
	pos += cnt;
    }

    if (pos)
    {
	/* if half a packet remains, move it down */
	if (pos < common->bufpos)
	{
 	    DBG(7, ErrorF("MOVE %d bytes\n", common->bufpos - pos));
	    memmove(common->buffer,common->buffer+pos, common->bufpos-pos);
	    common->bufpos -= pos;
	}

 	/* otherwise, reset the buffer for next time */
	else common->bufpos = 0;
    }
}

/*
 ***************************************************************************
 *
 * xf86WcmDevControlProc --
 *
 ***************************************************************************
 */
static void
xf86WcmDevControlProc(DeviceIntPtr device,PtrCtrl *ctrl)
{
    DBG(2, ErrorF("xf86WcmDevControlProc\n"));
}

/*****************************************************************************
 * xf86WcmInitDevice --
 *   Open and initialize the tablet
 ****************************************************************************/

static Bool xf86WcmInitDevice(LocalDevicePtr local)
{
    WacomCommonPtr common = ((WacomDevicePtr)local->private)->common;
    int loop;

    DBG(1,ErrorF("xf86WcmInitDevice: "));
    if (common->wcmInitialized)
    {
	DBG(1,ErrorF("already initialized\n"));
	return TRUE;
    }

    DBG(1,ErrorF("initializing\n"));

    /* attempt to open the device */
    if ((xf86WcmOpen(local) != Success) || (local->fd < 0))
    {
	DBG(1,ErrorF("Failed to open device (fd=%d)\n",local->fd));
	if (local->fd >= 0)
	{
	    DBG(1,ErrorF("Closing device\n"));
	    SYSCALL(xf86WcmClose(local->fd));
	}
	local->fd = -1;
	return FALSE;
    }

    /* on success, mark all other local devices as open and initialized */
    common->wcmInitialized = TRUE;

    DBG(1,ErrorF("Marking all devices open\n"));
    /* report the file descriptor to all devices */
    for (loop=0; loop<common->wcmNumDevices; loop++)
	common->wcmDevices[loop]->fd = local->fd;

    return TRUE;
}

/*****************************************************************************
 * xf86WcmDevClose --
 ****************************************************************************/

static void xf86WcmDevClose(LocalDevicePtr local)
{
    WacomDevicePtr priv = (WacomDevicePtr)local->private;
    WacomCommonPtr common = priv->common;
    int loop, num = 0;

    for (loop=0; loop<common->wcmNumDevices; loop++)
    {
	if (common->wcmDevices[loop]->fd >= 0)
	    num++;
    }
    DBG(4, ErrorF("Wacom number of open devices = %d\n", num));

    if (num == 1)
    {
	DBG(1,ErrorF("Closing device; uninitializing.\n"));
	SYSCALL(xf86WcmClose(local->fd));
	common->wcmInitialized = FALSE;
    }

    local->fd = -1;
}

int xf86WcmWait(int t)
{
    int err = xf86WaitForInput(-1, ((t) * 1000));
    if (err != -1) return Success;

    ErrorF("Wacom select error : %s\n", strerror(errno));
    return err;
}

int xf86WcmReady(int fd)
{
    int n = xf86WaitForInput(fd, 0);
    if (n >= 0) return n ? 1 : 0;
    ErrorF("Wacom select error : %s\n", strerror(errno));
    return 0;
}

/*
 ***************************************************************************
 *
 * xf86WcmOpen --
 *
 ***************************************************************************
 */
static Bool
xf86WcmOpen(LocalDevicePtr	local)
{
    WacomDevicePtr	priv = (WacomDevicePtr)local->private;
    WacomCommonPtr	common = priv->common;
    WacomDeviceClass** ppDevCls;
    
    DBG(1, ErrorF("opening %s\n", common->wcmDevice));

    local->fd = xf86WcmOpenTablet(local);
    if (local->fd < 0) {
	ErrorF("Error opening %s : %s\n", common->wcmDevice, strerror(errno));
	return !Success;
    }

    /* Detect device class; default is serial device */
    for (ppDevCls=wcmDeviceClasses; *ppDevCls!=NULL; ++ppDevCls)
    {
	if ((*ppDevCls)->Detect(local))
 	{
	    common->wcmDevCls = *ppDevCls;
	    break;
	}
    }

    /* Initialize the tablet */
    return common->wcmDevCls->Init(local);
}

/* reset raw data counters for filters */
static void resetSampleCounter(const WacomChannelPtr pChannel)
{
    /* if out of proximity, reset hardware filter */
    if (!pChannel->valid.state.proximity)
    {
	pChannel->nSamples = 0;
	pChannel->rawFilter.npoints = 0;
	pChannel->rawFilter.statex = 0;
	pChannel->rawFilter.statey = 0;
    }
}

/*****************************************************************************
** Transformations
*****************************************************************************/

static void transPressureCurve(WacomDevicePtr pDev, WacomDeviceStatePtr pState)
{
    if (pDev->pPressCurve)
    {
	int p = pState->pressure;

	/* clip */
	p = (p < 0) ? 0 : (p > pDev->common->wcmMaxZ) ? pDev->common->wcmMaxZ : p;

	/* rescale pressure to FILTER_PRESSURE_RES */
	p = (p * FILTER_PRESSURE_RES) / pDev->common->wcmMaxZ;

	/* apply pressure curve function */
	p = pDev->pPressCurve[p];

	/* scale back to wcmMaxZ */
	pState->pressure = (p * pDev->common->wcmMaxZ) / FILTER_PRESSURE_RES;
    }
}

static void commonDispatchDevice(WacomCommonPtr common,
        const WacomChannelPtr pChannel)
{
    int id, idx;
    WacomDevicePtr priv;
    LocalDevicePtr pDev = NULL;
    LocalDevicePtr pLastDev = pChannel->pDev;
    WacomDeviceState* ds = &pChannel->valid.states[0];
    WacomDeviceState* pLast = &pChannel->valid.states[1];

    DBG(10, ErrorF("commonDispatchEvents\n"));

    /* Find the device the current events are meant for */
    for (idx=0; idx<common->wcmNumDevices; idx++)
    {
	priv = common->wcmDevices[idx]->private;
	id = DEVICE_ID(priv->flags);

	if (id == ds->device_type &&
                        ((!priv->serial) || (ds->serial_num == priv->serial)))
	{
	    if ((priv->topX <= ds->x && priv->bottomX > ds->x &&
                                priv->topY <= ds->y && priv->bottomY > ds->y))
	    {
		DBG(11, ErrorF("tool id=%d for %s\n",
                                        id, common->wcmDevices[idx]->name));
		pDev = common->wcmDevices[idx];
		break;
	    }
	    /* Fallback to allow the cursor to move smoothly along screen edges */
	    else if (priv->oldProximity)
	    {
		pDev = common->wcmDevices[idx];
	    }
	}
    }

    DBG(11, ErrorF("commonDispatchEvents: %p %p\n", (void *)pDev, (void *)pLastDev));

    /* if the logical device of the same physical tool has changed,
     * send proximity out to the previous one */
    if (pLastDev && (pLastDev != pDev) && (pLast->serial_num == ds->serial_num))
    {
	pLast->proximity = 0;
	xf86WcmSendEvents(pLastDev, pLast);
    }

    /* if a device matched criteria, handle filtering per device
     * settings, and send event to XInput */
    if (pDev)
    {
	WacomDeviceState filtered = pChannel->valid.state;
	WacomDevicePtr priv = pDev->private;

	/* Device transformations come first */

	/* transform pressure */
	transPressureCurve(priv,&filtered);

 	/* User-requested filtering comes next */

	/* User-requested transformations come last */

#if 0

	/* not quite ready for prime-time;
	 * it needs to be possible to disable,
	 * and returning throttle to zero does
	 * not reset the wheel, yet. */

	int sampleTime, ticks;

	/* get the sample time */
	sampleTime = GetTimeInMillis();

	ticks = ThrottleToRate(ds->throttle);

	/* throttle filter */
 	if (!ticks)
	{
	    priv->throttleLimit = -1;
	}
	else if ((priv->throttleStart > sampleTime) ||
                        (priv->throttleLimit == -1))
	{
	    priv->throttleStart = sampleTime;
	    priv->throttleLimit = sampleTime + ticks;
	}
	else if (priv->throttleLimit < sampleTime)
	{
	    DBG(6, ErrorF("LIMIT REACHED: s=%d l=%d n=%d v=%d "
                                "N=%d\n", priv->throttleStart,
                                priv->throttleLimit, sampleTime,
                                ds->throttle, sampleTime + ticks));

	    ds->relwheel = (ds->throttle > 0) ? 1 :
                                (ds->throttle < 0) ? -1 : 0;

	    priv->throttleStart = sampleTime;
	    priv->throttleLimit = sampleTime + ticks;
	}
	else
	    priv->throttleLimit = priv->throttleStart + ticks;

#endif /* throttle */

	/* force out-prox when height is greater than 112.
	 * This only applies to USB protocol V tablets
	 * which aimed at improving relative movement support.
	 */
 	if (ds->distance > 112 && !(priv->flags & ABSOLUTE_FLAG))
	{
	    ds->proximity = 0;
	    filtered.proximity = 0;
	}

	xf86WcmSendEvents(pDev, &filtered);
    }

    /* otherwise, if no device matched... */
    else
    {
	DBG(11, ErrorF("no device matches with id=%d, serial=%d\n",
                                ds->device_type, ds->serial_num));
    }

    /* save the last device */
    pChannel->pDev = pDev;
}

/*****************************************************************************
 * xf86WcmEvent -
 *   Handles suppression, transformation, filtering, and event dispatch.
 ****************************************************************************/

void xf86WcmEvent(WacomCommonPtr common, unsigned int channel,
        const WacomDeviceState* pState)
{
    WacomDeviceState* pLast;
    WacomDeviceState ds;
    WacomChannelPtr pChannel;

    /* sanity check the channel */
    if (channel >= MAX_CHANNELS)
 	return;

    pChannel = common->wcmChannel + channel;
    pLast = &pChannel->valid.state;

    /* we must copy the state because certain types of filtering
     * will need to change the values (ie. for error correction) */
    ds = *pState;

    /* timestamp the state for velocity and acceleration analysis */
    ds.sample = GetTimeInMillis();

    DBG(10, ErrorF("xf86WcmEvent: c=%d i=%d t=%d s=0x%X x=%d y=%d b=0x%X "
                "p=%d rz=%d tx=%d ty=%d aw=%d rw=%d t=%d df=%d px=%d st=%d\n",
                channel,
                ds.device_id,
                ds.device_type,
                ds.serial_num,
                ds.x, ds.y, ds.buttons,
                ds.pressure, ds.rotation, ds.tiltx,
                ds.tilty, ds.abswheel, ds.relwheel, ds.throttle,
                ds.discard_first, ds.proximity, ds.sample));

    /* Filter raw data, fix hardware defects, perform error correction */
    if (RAW_FILTERING(common) && common->wcmModel->FilterRaw)
    {
	if (common->wcmModel->FilterRaw(common,pChannel,&ds))
	{
	    DBG(10, ErrorF("Raw filtering discarded data.\n"));
	    resetSampleCounter(pChannel);
	    return; /* discard */
	}
    }

    /* Discard unwanted data */
    if (xf86WcmSuppress(common->wcmSuppress, pLast, &ds))
    {
	DBG(10, ErrorF("Suppressing data according to filter\n"));

	/* If throttle is not in use, discard data. */
	if (ABS(ds.throttle) < common->wcmSuppress)
	{
	    resetSampleCounter(pChannel);
	    return;
	}

	/* Otherwise, we need this event for time-rate-of-change
	 * values like the throttle-to-relative-wheel filter.
	 * To eliminate position change events, we reset all values
	 * to last unsuppressed position. */

 	ds = *pLast;
	RESET_RELATIVE(ds);
    }

    /* JEJ - Do not move this code without discussing it with me.
     * The device state is invariant of any filtering performed below.
     * Changing the device state after this point can and will cause
     * a feedback loop resulting in oscillations, error amplification,
     * unnecessary quantization, and other annoying effects. */

    /* save channel device state and device to which last event went */
    memmove(pChannel->valid.states + 1, pChannel->valid.states,
                sizeof(WacomDeviceState) * (MAX_SAMPLES - 1));
    pChannel->valid.state = ds; /*save last raw sample */
    if (pChannel->nSamples < 4) ++pChannel->nSamples;

    commonDispatchDevice(common,pChannel);

    resetSampleCounter(pChannel);
}

/*****************************************************************************
 * xf86WcmInitTablet -- common initialization for all tablets
 ****************************************************************************/

int xf86WcmInitTablet(WacomCommonPtr common, WacomModelPtr model,
        int fd, const char* id, float version)
{
    /* Initialize the tablet */
    model->Initialize(common,fd,id,version);

    /* Get tablet resolution */
    if (model->GetResolution) model->GetResolution(common,fd);

    /* Get tablet range */
    if (model->GetRanges && (model->GetRanges(common,fd) != Success))
	return !Success;

    /* Default threshold value if not set */
    if (common->wcmThreshold <= 0)
    {
	/* Threshold for counting pressure as a button */
	common->wcmThreshold = common->wcmMaxZ * 3 / 50;
        ErrorF("%s Wacom using pressure threshold of %d for button 1\n",
                        XCONFIG_PROBED, common->wcmThreshold);
    }

    /* Reset tablet to known state */
    if (model->Reset && (model->Reset(common,fd) != Success))
    {
 	ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
	return !Success;
    }

    /* Enable tilt mode, if requested and available */
    if ((common->wcmFlags & TILT_REQUEST_FLAG) && model->EnableTilt)
	if (model->EnableTilt(common,fd) != Success) return !Success;

    /* Enable hardware suppress, if requested and available */
    if ((common->wcmSuppress != 0) && model->EnableSuppress)
	if (model->EnableSuppress(common,fd) != Success) return !Success;

    /* change the serial speed, if requested */
    if (common->wcmLinkSpeed != 9600) {
	if (model->SetLinkSpeed) {
	    if (model->SetLinkSpeed(common,fd) != Success) return !Success;
	    else
		ErrorF("Tablet does not support setting link "
                                "speed, or not yet implemented\n");
	}
    }

    /* output tablet state as probed */
    if (xf86Verbose)
	ErrorF("%s Wacom %s tablet speed=%d maxX=%d maxY=%d maxZ=%d "
                        "resX=%d resY=%d suppress=%d tilt=%s\n",
                        XCONFIG_PROBED,
                        model->name, common->wcmLinkSpeed,
                        common->wcmMaxX, common->wcmMaxY, common->wcmMaxZ,
                        common->wcmResolX, common->wcmResolY,
                        common->wcmSuppress,
                        HANDLE_TILT(common) ? "enabled" : "disabled");

    /* start the tablet data */
    if (model->Start && (model->Start(common,fd) != Success)) return !Success;

    /*set the model */
    common->wcmModel = model;

    return Success;
}

/*
 ***************************************************************************
 *
 * xf86WcmDevOpen --
 *	Open the physical device and init information structs.
 *
 ***************************************************************************
 */
static int
xf86WcmDevOpen(DeviceIntPtr       pWcm)
{
    LocalDevicePtr	local = (LocalDevicePtr)pWcm->public.devicePrivate;
    WacomDevicePtr	priv = (WacomDevicePtr)PRIVATE(pWcm);
    WacomCommonPtr	common = priv->common;
    double		screenRatio, tabletRatio;
    int 		totalWidth = 0, maxHeight = 0;
    
    if (local->fd < 0) {
	if (!xf86WcmInitDevice(local) || (local->fd < 0))
	{
	    DBG(1,ErrorF("Failed to initialize device\n"));
	    return FALSE;
	}
    }

    if (priv->factorX == 0.0) {
	
	if (priv->twinview != TV_NONE && priv->bottomX == 0 && 
		priv->bottomY == 0 && priv->topX == 0 && priv->topY == 0)
	{
	    if (IsCursor(priv))
	    {
		/* for absolute cursor */
		priv->topX = 80;
		priv->topY = 80;
	    }
	    else
	    {
		/* for absolute stylus and eraser */
		priv->topX = 50;
		priv->topY = 50;
	    }
	    priv->bottomX = common->wcmMaxX - priv->topX;
	    priv->bottomY = common->wcmMaxY - priv->topY;
	}
	if (priv->bottomX == 0) priv->bottomX = common->wcmMaxX;

	if (priv->bottomY == 0) priv->bottomY = common->wcmMaxY;

	/* Verify Box validity */

	if (priv->topX > common->wcmMaxX) {
	    ErrorF("Wacom invalid TopX (%d) reseting to 0\n", priv->topX);
	    priv->topX = 0;
	}

	if (priv->topY > common->wcmMaxY) {
	    ErrorF("Wacom invalid TopY (%d) reseting to 0\n", priv->topY);
	    priv->topY = 0;
	}

	if (priv->bottomX < priv->topX) {
	    ErrorF("Wacom invalid BottomX (%d) reseting to %d\n",
		   priv->bottomX, common->wcmMaxX);
	    priv->bottomX = common->wcmMaxX;
	}

	if (priv->bottomY < priv->topY) {
	    ErrorF("Wacom invalid BottomY (%d) reseting to %d\n",
		   priv->bottomY, common->wcmMaxY);
	    priv->bottomY = common->wcmMaxY;
	}

	if (priv->screen_no != -1 && (priv->screen_no >= priv->numScreen ||
		priv->screen_no < 0)) {
	    if (priv->twinview == TV_NONE || priv->screen_no != 1)
	    {
		ErrorF("%s: invalid screen number %d, resetting to 0\n",
			local->name, priv->screen_no);
		priv->screen_no = 0;
	    }
	}

	/* Calculate the ratio according to KeepShape, TopX and TopY */

	if (priv->screen_no != -1) {
	    priv->currentScreen = priv->screen_no;
	    if (priv->twinview == TV_NONE)
	    {
		totalWidth = screenInfo.screens[priv->currentScreen]->width;
		maxHeight = screenInfo.screens[priv->currentScreen]->height;
	    }
	    else
	    {
		totalWidth = priv->tvResolution[2*priv->currentScreen];
		maxHeight = priv->tvResolution[2*priv->currentScreen+1];
	    }
	}
	else
	{
	    int i;
	    for (i = 0; i < priv->numScreen; i++)
	    {
		totalWidth += screenInfo.screens[i]->width;
		if (maxHeight < screenInfo.screens[i]->height)
		    maxHeight=screenInfo.screens[i]->height;
	    }
	}
	
	if (priv->flags & KEEP_SHAPE_FLAG) {
	    screenRatio = (double)totalWidth / (double)maxHeight;

	    tabletRatio = ((double) (common->wcmMaxX - priv->topX))
		/ (common->wcmMaxY - priv->topY);

	    DBG(2, ErrorF("screenRatio = %.3g, tabletRatio = %.3g\n",
			  screenRatio, tabletRatio));

	    if (screenRatio > tabletRatio) {
		priv->bottomX = common->wcmMaxX;
		priv->bottomY = (common->wcmMaxY - priv->topY) *
 			tabletRatio / screenRatio + priv->topY;
	    } else {
		priv->bottomX = (common->wcmMaxX - priv->topX) *
			 screenRatio / tabletRatio + priv->topX;
		priv->bottomY = common->wcmMaxY;
	    }
	}
	if (priv->numScreen == 1)
	{
	    priv->factorX = totalWidth / (double)(priv->bottomX - priv->topX);
	    priv->factorY = maxHeight / (double)(priv->bottomY - priv->topY);
	    DBG(2, ErrorF("X factor = %.3g, Y factor = %.3g\n",
                                priv->factorX, priv->factorY));
	}
    
	if (xf86Verbose)
	    ErrorF("%s Wacom tablet top X=%d top Y=%d "
		   "bottom X=%d bottom Y=%d\n",
		   XCONFIG_PROBED, priv->topX, priv->topY,
		   priv->bottomX, priv->bottomY);
    }

	/* x and y axes */
    InitValuatorAxisStruct(pWcm, 0, 0,
			((priv->bottomX - priv->topX) * priv->dscaleX), /* max val */
			mils(common->wcmResolX), /* tablet resolution */
			0, mils(common->wcmResolX)); /* max_res */

    InitValuatorAxisStruct(pWcm, 1, 0,
			((priv->bottomY - priv->topY) * priv->dscaleY), /* max val */
			mils(common->wcmResolY), /* tablet resolution */
			0, mils(common->wcmResolY)); /* max_res */

	/* pressure */
    InitValuatorAxisStruct(pWcm, 2, 0, common->wcmMaxZ, 1, 1, 1);

    if (IsCursor(priv))
    {
	/* z-rot and throttle */
    	InitValuatorAxisStruct(pWcm, 3, -900, 899, 1, 1, 1);
    	InitValuatorAxisStruct(pWcm, 4, -1023, 1023, 1, 1, 1);
    }
    else
    {
	/* tilt-x and tilt-y */
    	InitValuatorAxisStruct(pWcm, 3, -64, 63, 1, 1, 1);
    	InitValuatorAxisStruct(pWcm, 4, -64, 63, 1, 1, 1);
    }

	/* wheel */
    InitValuatorAxisStruct(pWcm, 5, 0, 1023, 1, 1, 1);
    
    return (local->fd != -1);
}

/*
 ***************************************************************************
 *
 * xf86WcmDevProc --
 *      Handle the initialization, etc. of a wacom
 *
 ***************************************************************************
 */
static int
xf86WcmDevProc(DeviceIntPtr       pWcm,
	    int                what)
{
    CARD8                 map[(32 << 4) + 1];
    int                   nbaxes;
    int                   nbbuttons;
    int                   loop;
    LocalDevicePtr        local = (LocalDevicePtr)pWcm->public.devicePrivate;
    WacomDevicePtr        priv = (WacomDevicePtr)PRIVATE(pWcm);
  
    DBG(2, ErrorF("BEGIN xf86WcmProc dev=%p priv=%p type=%s flags=%d what=%d\n",
		  (void *)pWcm, (void *)priv, 
		  (DEVICE_ID(priv->flags) == STYLUS_ID) ? "stylus" :
		  (DEVICE_ID(priv->flags) == CURSOR_ID) ? "cursor" : "eraser",
		  priv->flags, what));

    switch (what)
	{
	case DEVICE_INIT: 
	    DBG(1, ErrorF("xf86WcmProc pWcm=%p what=INIT\n", (void *)pWcm));
      
	    nbaxes = 6;		/* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */
	    
	    switch(DEVICE_ID(priv->flags)) {
	    case ERASER_ID:
		nbbuttons = 1;
		break;
	    case STYLUS_ID:
		nbbuttons = 4;
		break;
	    default:
		nbbuttons = 16;
		break;
	    }
	    
	    for(loop=1; loop<=nbbuttons; loop++) map[loop] = loop;

	    if (InitButtonClassDeviceStruct(pWcm,
					    nbbuttons,
					    map) == FALSE) {
		ErrorF("unable to allocate Button class device\n");
		return !Success;
	    }
      
	    if (InitFocusClassDeviceStruct(pWcm) == FALSE) {
		ErrorF("unable to init Focus class device\n");
		return !Success;
	    }
          
	    if (InitPtrFeedbackClassDeviceStruct(pWcm,
						 xf86WcmDevControlProc) == FALSE) {
		ErrorF("unable to init ptr feedback\n");
		return !Success;
	    }
	    
	    if (InitProximityClassDeviceStruct(pWcm) == FALSE) {
		ErrorF("unable to init proximity class device\n");
		return !Success;
	    }

	    if (InitKeyClassDeviceStruct(pWcm, &wacom_keysyms, NULL) == FALSE) {
		ErrorF("unable to init key class device\n"); 
		return !Success;
	    }

	    if (InitValuatorClassDeviceStruct(pWcm, 
					      nbaxes,
					      xf86GetMotionEvents, 
					      local->history_size,
					      ((priv->flags & ABSOLUTE_FLAG) 
					      ? Absolute : Relative) |
					      OutOfProximity)
		== FALSE) {
		ErrorF("unable to allocate Valuator class device\n"); 
		return !Success;
	    }
	    else {
		/* allocate the motion history buffer if needed */
		xf86MotionHistoryAllocate(local);
	    }

	    /* open the device to gather informations */
	    if (!xf86WcmDevOpen(pWcm))
	    {
		/* Sometime PL does not open the first time */
                DBG(1, ErrorF("xf86WcmProc try to open pWcm=%p again\n", (void *)pWcm));
		if (!xf86WcmDevOpen(pWcm)) {
                    DBG(1, ErrorF("xf86WcmProc pWcm=%p what=INIT FALSE\n", (void *)pWcm));
		    return !Success;
		}
	    }
	    break; 
      
	case DEVICE_ON:
	    DBG(1, ErrorF("xf86WcmProc pWcm=%p what=ON\n", (void *)pWcm));

	    if ((local->fd < 0) && (!xf86WcmDevOpen(pWcm))) {
		pWcm->inited = FALSE;
		return !Success;
	    }
	    xf86AddEnabledDevice(local);
	    pWcm->public.on = TRUE;
	    break;
      
	case DEVICE_OFF:
	case DEVICE_CLOSE:
	    DBG(1, ErrorF("xf86WcmProc  pWcm=%p what=%s\n", (void *)pWcm,
			  (what == DEVICE_CLOSE) ? "CLOSE" : "OFF"));
	    if (local->fd >= 0) {
		xf86RemoveEnabledDevice(local);
		xf86WcmDevClose(local);
	    }
	    pWcm->public.on = FALSE;
	    break;
	    
	default:
	    ErrorF("wacom unsupported mode=%d\n", what);
	    return !Success;
	    break;
	}
    DBG(2, ErrorF("END   xf86WcmProc Success what=%d dev=%p priv=%p\n",
		  what, (void *)pWcm, (void *)priv));
    return Success;
}

/*****************************************************************************
 * xf86WcmSetParam
 ****************************************************************************/

static int xf86WcmSetParam(LocalDevicePtr local, int param, int value)
{
    WacomDevicePtr priv = (WacomDevicePtr)local->private;
    char st[32];

    switch (param) {
	case XWACOM_PARAM_TOPX:
	    xf86ReplaceIntOption(local->options, "TopX", value);
	    priv->topX = xf86SetIntOption(local->options, "TopX", 0);
	    break;
	case XWACOM_PARAM_TOPY:
	    xf86ReplaceIntOption(local->options, "TopY", value);
	    priv->topY = xf86SetIntOption(local->options, "TopY", 0);
	    break;
	case XWACOM_PARAM_BOTTOMX:
	    xf86ReplaceIntOption(local->options, "BottomX", value);
	    priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
	    break;
	case XWACOM_PARAM_BOTTOMY:
	    xf86ReplaceIntOption(local->options, "BottomY", value);
	    priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
	    break;
	case XWACOM_PARAM_BUTTON1:
	    if ((value < 0) || (value > 18)) return BadValue;
	    xf86ReplaceIntOption(local->options,"Button1",value);
	    priv->button[0] = xf86SetIntOption(local->options,"Button1",1);
	    break;
	case XWACOM_PARAM_BUTTON2:
	    if ((value < 0) || (value > 18)) return BadValue;
	    xf86ReplaceIntOption(local->options, "Button2", value);
	    priv->button[1] = xf86SetIntOption(local->options,"Button2",2);
	    break;
	case XWACOM_PARAM_BUTTON3:
	    if ((value < 0) || (value > 18)) return BadValue;
	    xf86ReplaceIntOption(local->options, "Button3", value);
	    priv->button[2] = xf86SetIntOption(local->options,"Button3",3);
	    break;
	case XWACOM_PARAM_BUTTON4:
	    if ((value < 0) || (value > 18)) return BadValue;
	    xf86ReplaceIntOption(local->options, "Button4", value);
	    priv->button[3] = xf86SetIntOption(local->options,"Button4",4);
	    break;
	case XWACOM_PARAM_BUTTON5:
	    if ((value < 0) || (value > 18)) return BadValue;
	    xf86ReplaceIntOption(local->options, "Button5", value);
	    priv->button[4] = xf86SetIntOption(local->options,"Button5",5);
	    break;
	case XWACOM_PARAM_DEBUGLEVEL:
	    if ((value < 0) || (value > 100)) return BadValue;
	    xf86ReplaceIntOption(local->options, "DebugLevel", value);
	    gWacomModule.debugLevel = value;
	    break;
	case XWACOM_PARAM_RAWFILTER:
	    if ((value < 0) || (value > 1)) return BadValue;
	    xf86ReplaceIntOption(local->options, "RawFilter", value);
	    if (value) priv->common->wcmFlags |= RAW_FILTERING_FLAG;
	    else priv->common->wcmFlags &= ~(RAW_FILTERING_FLAG);
	    break;
	case XWACOM_PARAM_PRESSCURVE: {
	    char chBuf[64];
	    int x0 = (value >> 24) & 0xFF;
	    int y0 = (value >> 16) & 0xFF;
	    int x1 = (value >> 8) & 0xFF;
	    int y1 = value & 0xFF;
	    if ((x0 > 100) || (y0 > 100) || (x1 > 100) || (y1 > 100))
		return BadValue;
	    snprintf(chBuf,sizeof(chBuf),"%d %d %d %d",x0,y0,x1,y1);
	    xf86ReplaceStrOption(local->options, "PressCurve",chBuf);
	    xf86WcmSetPressureCurve(priv,x0,y0,x1,y1);
	    break;
	}
	case XWACOM_PARAM_MODE:
	    if ((value < 0) || (value > 1)) return BadValue;
	    if (value) {
		priv->flags |= ABSOLUTE_FLAG;
		xf86ReplaceStrOption(local->options, "Mode", "Absolute");
	    } else {
		priv->flags &= ~ABSOLUTE_FLAG;
		xf86ReplaceStrOption(local->options, "Mode", "Relative");
	    }
	    break;
	case XWACOM_PARAM_SPEEDLEVEL:
	    if ((value < 0) || (value > 10)) return BadValue;
	    if (value > 5) priv->speed = 2.00*((double)value - 5.00);
	    else priv->speed = ((double)value + 1.00) / 6.00;
	    sprintf(st, "%.3f", priv->speed);
	    xf86AddNewOption(local->options, "Speed", st);
	    break;
	case XWACOM_PARAM_ACCEL:
	    if ((value < 0) || (value > MAX_ACCEL-1)) return BadValue;
	    priv->accel = value;
	    xf86ReplaceIntOption(local->options, "Accel", priv->accel);
	    break;
	case XWACOM_PARAM_CLICKFORCE:
	    if ((value < 0) || (value > 20)) return BadValue;
		 priv->common->wcmThreshold = 
			(int)((double)(value*priv->common->wcmMaxZ)/100.00+0.5);
	    xf86ReplaceIntOption(local->options, "Threshold",
                                priv->common->wcmThreshold);
	    break;
	case XWACOM_PARAM_XYDEFAULT:
	    xf86ReplaceIntOption(local->options, "TopX", 0);
	    priv->topX = xf86SetIntOption(local->options, "TopX", 0);
	    xf86ReplaceIntOption(local->options, "TopY", 0);
	    priv->topY = xf86SetIntOption(local->options, "TopY", 0);
	    xf86ReplaceIntOption(local->options,
                                "BottomX", priv->common->wcmMaxX);
	    priv->bottomX = xf86SetIntOption(local->options,
                                "BottomX", priv->common->wcmMaxX);
	    xf86ReplaceIntOption(local->options,
                                "BottomY", priv->common->wcmMaxY);
	    priv->bottomY = xf86SetIntOption(local->options,
                                "BottomY", priv->common->wcmMaxY);
	    break;
	case XWACOM_PARAM_GIMP:
	    if ((value != 0) && (value != 1)) return BadValue;
		priv->common->wcmGimp = value;
	    break;
	default:
	    DBG(10, ErrorF("xf86WcmSetParam invalid param %d\n",param));
	    return BadMatch;
    }
    return Success;
}

/*****************************************************************************
 * xf86WcmOptionCommandToFile
 ****************************************************************************/

static int xf86WcmOptionCommandToFile(LocalDevicePtr local)
{
    WacomDevicePtr  priv = (WacomDevicePtr)local->private;
    char            fileName[80] = "/etc/X11/wcm.";
    char            command[256];
    FILE            *fp = 0;
    int             value;
    double          speed;
    char	    *s;

    strcat(fileName, local->name);
    fp = fopen(fileName, "w+");
    if ( fp ) {
	/* write user defined options as xsetwacom commands into fp */
	s = xf86FindOptionValue(local->options, "TopX");
	if ( s && priv->topX )
	    fprintf(fp, "xsetwacom set %s TopX %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "TopY");
	if ( s && priv->topY )
	    fprintf(fp, "xsetwacom set %s TopY %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "BottomX");
	if ( s && priv->bottomX != priv->common->wcmMaxX )
	    fprintf(fp, "xsetwacom set %s BottomX %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "BottomY");
	if ( s && priv->bottomY != priv->common->wcmMaxY )
	    fprintf(fp, "xsetwacom set %s BottomY %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Button1");
	if ( s && priv->button[0] != 1 )
	    fprintf(fp, "xsetwacom set %s Button1 %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Button2");
	if ( s && priv->button[1] != 2 )
	    fprintf(fp, "xsetwacom set %s Button2 %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Button3");
	if ( s && priv->button[2] != 3 )
	    fprintf(fp, "xsetwacom set %s Button3 %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Button4");
	if ( s && priv->button[3] != 4 )
	    fprintf(fp, "xsetwacom set %s Button4 %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Button5");
	if ( s && priv->button[4] != 5 )
	    fprintf(fp, "xsetwacom set %s Button5 %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "PressCurve");
	if ( s && !IsCursor(priv) )
	    fprintf(fp, "xsetwacom set %s PressCurve %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Mode");
	if ( s && (((priv->flags & ABSOLUTE_FLAG) && IsCursor(priv))
		|| (!(priv->flags & ABSOLUTE_FLAG) && !IsCursor(priv))))
	    fprintf(fp, "xsetwacom set %s Mode %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "RawFilter");
	if ( s )
	    fprintf(fp, "xsetwacom set %s RawFilter %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Accel");
	if ( s && priv->accel )
	    fprintf(fp, "xsetwacom set %s Accel %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Suppress");
	if ( s )
	    fprintf(fp, "xsetwacom set %s Suppress %s\n", local->name, s);

	s = xf86FindOptionValue(local->options, "Speed");
	if ( s && priv->speed != DEFAULT_SPEED )
	{
	     speed = strtod(s, NULL);
	     if(speed > 10.00) value = 10;
	     else if(speed >= 1.00) value = (int)(speed/2.00 + 5.00);
	     else if(speed < (double)(1.00/6.00)) value = 0;
	     else value = (int)(speed*6.00 - 0.50);
	     fprintf(fp, "xsetwacom set %s SpeedLevel %d\n", local->name, value);
	}

	s = xf86FindOptionValue(local->options, "Threshold");
	if ( s )
	{
	    value = atoi(s);
	    value = (int)((double)value*100.00/(double)priv->common->wcmMaxZ+0.5);
	    fprintf(fp, "xsetwacom set %s ClickForce %d\n", local->name, value);
	}

	fprintf(fp, "%s", "default TopX 0\n");
	fprintf(fp, "%s", "default TopY 0\n");
	fprintf(fp, "default BottomX %d\n", priv->common->wcmMaxX);
 	fprintf(fp, "default BottomY %d\n", priv->common->wcmMaxY);
	if (IsCursor(priv))
	    sprintf(command, "default Mode Relative\n");
 	else
	    sprintf(command, "default Mode Absolute\n");
	fprintf(fp, "%s", command);
	fprintf(fp, "%s", "default SpeedLevel 5\n");
	fprintf(fp, "%s", "default ClickForce 6\n");
	fprintf(fp, "%s", "default Accel 0\n");
	fclose(fp);
   }
   return(Success);
}

/*****************************************************************************
 * xf86WcmModelToFile
 ****************************************************************************/

static int xf86WcmModelToFile(LocalDevicePtr local)
{
    FILE            *fp = 0;
    LocalDevicePtr  localDevices = xf86FirstLocalDevice();
    WacomDevicePtr  priv = NULL, lprv;
    char            m1[32], m2[32], *m3;
    int             i = 0, x = 0, y = 0;

    fp = fopen("/etc/wacom.dat", "w+");
    if ( fp ) {
	while(localDevices) {
            m3 = xf86FindOptionValue(localDevices->options, "Type");
            if (m3 && (strstr(m3, "eraser") || strstr(m3, "stylus") 
			|| strstr(m3, "cursor")))
		lprv = (WacomDevicePtr)localDevices->private;
            else
                lprv = NULL;
	    if (lprv && lprv->common) {
		sscanf((char*)(lprv->common->wcmModel)->name, "%s %s", m1, m2);
		fprintf(fp, "%s %s %s\n", localDevices->name, m2, m3);
	    	if (lprv->twinview != TV_NONE) {
		    priv = lprv;
 	    	}
		if( !priv ) priv = lprv;
	    }
	    localDevices = localDevices->next;
	}
	/* write TwinView ScreenInfo */
	if (priv->twinview == TV_ABOVE_BELOW) {
	    fprintf(fp, "Screen0 %d %d %d %d\n", priv->tvResolution[0],
                                priv->tvResolution[1], 0, 0);
	    fprintf(fp, "Screen1 %d %d %d %d\n", priv->tvResolution[2],
                                priv->tvResolution[3], 0, priv->tvResolution[1]);
	} else if (priv->twinview == TV_LEFT_RIGHT) {
	    fprintf(fp, "Screen0 %d %d %d %d\n", priv->tvResolution[0],
                                priv->tvResolution[1], 0, 0);
	    fprintf(fp, "Screen1 %d %d %d %d\n", priv->tvResolution[2],
                                priv->tvResolution[3], priv->tvResolution[0], 0);
	} else {	/* write other screen setup info */
	    for (i = 0; i<screenInfo.numScreens; i++) {
		fprintf(fp, "Screen%d %d %d %d %d\n",
                                i, screenInfo.screens[i]->width,
                                screenInfo.screens[i]->height, x, y);
		x += screenInfo.screens[i]->width;
	     }
	}
	fclose(fp);
    }
    return(Success);
}


/*
 ***************************************************************************
 *
 * xf86WcmDevChangeControl --
 *
 ***************************************************************************
 */
static int
xf86WcmDevChangeControl(LocalDevicePtr	local,
		     xDeviceCtl		*control)
{
    xDeviceResolutionCtl* res = (xDeviceResolutionCtl *)control;
    int* r = (int*)(res+1);
    int param = r[0], value = r[1];

    DBG(10, ErrorF("xf86WcmDevChangeControl firstValuator=%d\n", res->first_valuator));

    if (control->control != DEVICE_RESOLUTION || !res->num_valuators)
	return BadMatch;

    r[0] = 1, r[1] = 1;
    switch (res->first_valuator) {
	case 0:  /* a new write to wcm.$name */
	{
	    return xf86WcmOptionCommandToFile(local);
	}
	case 1: /* a new write to wacom.dat */
	{
	    return xf86WcmModelToFile(local);
	}
	case 4:
	{
	    DBG(10,ErrorF("xf86WcmDevChangeControl: 0x%x, 0x%x\n", param, value));
	    return xf86WcmSetParam(local,param,value);
	}
	default:
	    DBG(10,ErrorF("xf86WcmDevChangeControl invalid "
                                "firstValuator=%d\n",res->first_valuator));
 	    return BadMatch;
    }
}

/*
 ***************************************************************************
 *
 * xf86WcmDevSwitchMode --
 *
 ***************************************************************************
 */
static int
xf86WcmDevSwitchMode(ClientPtr	client,
		  DeviceIntPtr	dev,
		  int		mode)
{
    LocalDevicePtr        local = (LocalDevicePtr)dev->public.devicePrivate;
    WacomDevicePtr        priv = (WacomDevicePtr)local->private;

    DBG(3, ErrorF("xf86WcmSwitchMode dev=%p mode=%d\n", (void *)dev, mode));
  
    if (mode == Absolute) {
	priv->flags |= ABSOLUTE_FLAG;
    }
    else {
	if (mode == Relative) {
	    priv->flags &= ~ABSOLUTE_FLAG; 
	}
	else {
	    DBG(1, ErrorF("xf86WcmDevSwitchMode dev=%p invalid mode=%d\n", 
			(void *)dev, mode));
	    return BadMatch;
	}
    }
    return Success;
}

/*
 ***************************************************************************
 *
 * xf86WcmAllocate --
 *
 ***************************************************************************
 */
static LocalDevicePtr
xf86WcmAllocate(char *  name,
                int     flag)
{
    LocalDevicePtr        local;
    WacomDevicePtr        priv;
    WacomCommonPtr        common;
    int			  i;

    priv = (WacomDevicePtr) xalloc(sizeof(WacomDeviceRec));
    if (!priv)
	return NULL;

    common = (WacomCommonPtr) xalloc(sizeof(WacomCommonRec));
    if (!common) {
	xfree(priv);
	return NULL;
    }

    local = xf86AllocateInput(gWacomModule.v4.wcmDrv, 0);
    if (!local) {
	xfree(priv);
	xfree(common);
	return NULL;
    }

    local->name = name;
    local->flags = 0;
    local->device_control = gWacomModule.DevProc;
    local->read_input = gWacomModule.DevReadInput;
    local->control_proc = gWacomModule.DevChangeControl;
    local->close_proc = gWacomModule.DevClose;
    local->switch_mode = gWacomModule.DevSwitchMode;
    local->conversion_proc = gWacomModule.DevConvert;
    local->reverse_conversion_proc = gWacomModule.DevReverseConvert;
    local->fd = -1;
    local->atom = 0;
    local->dev = NULL;
    local->private = priv;
    local->private_flags = 0;
    local->history_size  = 0;
    local->old_x = -1;
    local->old_y = -1;
    
    memset(priv,0,sizeof(*priv));
    priv->flags = flag;			/* various flags (device type, absolute...) */
    priv->oldX = -1;			/* previous X position */
    priv->oldY = -1;			/* previous Y position */
    priv->oldZ = -1;			/* previous pressure */
    priv->oldTiltX = -1;		/* previous tilt in x direction */
    priv->oldTiltY = -1;		/* previous tilt in y direction */
    priv->oldButtons = 0;		/* previous buttons state */
    priv->oldWheel = 0;			/* previous wheel */
    priv->topX = 0;			/* X top */
    priv->topY = 0;			/* Y top */
    priv->bottomX = 0;			/* X bottom */
    priv->bottomY = 0;			/* Y bottom */
    priv->factorX = 0.0;		/* X factor */
    priv->factorY = 0.0;		/* Y factor */
    priv->common = common;		/* common info pointer */
    priv->oldProximity = 0;		/* previous proximity */
    priv->serial = 0;		        /* serial number */
    priv->screen_no = -1;		/* associated screen */
    priv->speed = DEFAULT_SPEED;	/* rel. mode speed */
    priv->accel = 0; 			/* rel. mode acceleration */
    for (i=0; i<16; i++)
	priv->button[i] = i+1;		/* button i value */
    priv->numScreen = screenInfo.numScreens; /* number of configureed screens */
    priv->currentScreen = 0;		/* current screen in display */
    priv->dscaleX = 1.0;		/* dual screen scale X factor */
    priv->dscaleY = 1.0;		/* dual screen scale Y factor */
    priv->doffsetX = 0;			/* dual screen offset X */
    priv->doffsetY = 0;			/* dual screen offset Y */
    priv->twinview = TV_NONE;		/* not using twinview gfx */
    for (i=0; i<4; i++)
	priv->tvResolution[i] = 0;      /* unconfigured twinview resolution */
    priv->throttleValue = 0;
    priv->throttleStart = 0;
    priv->throttleLimit = -1;
    memset(common,0,sizeof(*common));
    memset(common->wcmChannel, 0, sizeof(common->wcmChannel));
    common->wcmDevice = "";		/* device file name */
    common->wcmSuppress = DEFAULT_SUPPRESS; /* transmit position if increment is superior */
    common->wcmFlags = RAW_FILTERING_FLAG;/* various flags */
    common->wcmDevices = (LocalDevicePtr*) xalloc(sizeof(LocalDevicePtr));
    common->wcmDevices[0] = local;
    common->wcmNumDevices = 1;		/* number of devices */
    common->wcmMaxX = 0;		/* max X value */
    common->wcmMaxY = 0;		/* max Y value */
    common->wcmMaxZ = 0;		/* max Z value */
    common->wcmResolX = 0;		/* X resolution in points/inch */
    common->wcmResolY = 0;		/* Y resolution in points/inch */
    common->wcmChannelCnt = 1;		/* number of channels */
    common->wcmProtocolLevel = 4;	/* protocol level */
    common->wcmThreshold = 0;		/* button 1 threshold for some tablet models */
    common->wcmInitialized = FALSE;	/* device is not initialized */
    common->wcmLinkSpeed = 9600;        /* serial link speed */
    common->wcmDevCls = &gWacomSerialDevice; /* device-specific functions */
    common->wcmModel = NULL;		/* model-specific functions */
    common->wcmGimp = 1;		/* enabled (=1) to support Gimp when Xinerama Enabled in multi-monitor desktop. Needs to be disabled (=0) for Cintiq calibration */
    return local;
}

/*
 ***************************************************************************
 *
 * xf86WcmAllocateStylus --
 *
 ***************************************************************************
 */
static LocalDevicePtr
xf86WcmAllocateStylus()
{
    LocalDevicePtr        local = xf86WcmAllocate(XI_STYLUS, STYLUS_ID);

    if (local)
	local->type_name = "Wacom Stylus";
    return local;
}

/*
 ***************************************************************************
 *
 * xf86WcmAllocateCursor --
 *
 ***************************************************************************
 */
static LocalDevicePtr
xf86WcmAllocateCursor()
{
    LocalDevicePtr        local = xf86WcmAllocate(XI_CURSOR, CURSOR_ID);

    if (local)
	local->type_name = "Wacom Cursor";
    return local;
}

/*
 ***************************************************************************
 *
 * xf86WcmAllocateEraser --
 *
 ***************************************************************************
 */
static LocalDevicePtr
xf86WcmAllocateEraser()
{
    LocalDevicePtr        local = xf86WcmAllocate(XI_ERASER, ABSOLUTE_FLAG|ERASER_ID);

    if (local)
	local->type_name = "Wacom Eraser";
    return local;
}

/*
 * xf86WcmUninit --
 *
 * called when the device is no longer needed.
 */
static void
xf86WcmUninit(InputDriverPtr	drv,
	      LocalDevicePtr	local,
	      int flags)
{
    WacomDevicePtr	priv;

    priv = (WacomDevicePtr) local->private;
    
    DBG(1, ErrorF("xf86WcmUninit\n"));
    
    gWacomModule.DevProc(local->dev, DEVICE_OFF);
    
    /* free pressure curve */
    if (priv->pPressCurve)
	xfree(priv->pPressCurve);

    xfree (priv);
    xf86DeleteInput(local, 0);    
}

/* xf86WcmMatchDevice - locate matching device and merge common structure */

static Bool xf86WcmMatchDevice(LocalDevicePtr pMatch, LocalDevicePtr pLocal)
{
    WacomDevicePtr privMatch = (WacomDevicePtr)pMatch->private;
    WacomDevicePtr priv = (WacomDevicePtr)pLocal->private;
    WacomCommonPtr common = priv->common;

    if ((pLocal != pMatch) && (pMatch->device_control == gWacomModule.DevProc) &&
                !strcmp(privMatch->common->wcmDevice, common->wcmDevice)) {
	DBG(2, ErrorF("xf86WcmInit wacom port share between"
                                " %s and %s\n", pLocal->name, pMatch->name));
	xfree(common->wcmDevices);
	xfree(common);
	common = priv->common = privMatch->common;
	common->wcmNumDevices++;
	common->wcmDevices = (LocalDevicePtr *)xrealloc(
                                common->wcmDevices,
                                sizeof(LocalDevicePtr) * common->wcmNumDevices);
	common->wcmDevices[common->wcmNumDevices - 1] = pLocal;
	return 1;
    }
    return 0;
}

/*
 * xf86WcmInit --
 *
 * called when the module subsection is found in XF86Config
 */
static InputInfoPtr
xf86WcmInit(InputDriverPtr	drv,
	    IDevPtr		dev,
	    int			flags)
{
    LocalDevicePtr	local = NULL;
    LocalDevicePtr	fakeLocal = NULL;
    WacomDevicePtr	priv = NULL;
    WacomCommonPtr	common = NULL;
    char		*s, b[10];
    int			i, oldButton;
    LocalDevicePtr	localDevices;

    gWacomModule.v4.wcmDrv = drv;

    fakeLocal = (LocalDevicePtr) xcalloc(1, sizeof(LocalDeviceRec));
    if (!fakeLocal)
	return NULL;

    fakeLocal->conf_idev = dev;

    /* Force default serial port options to exist because the serial init
     * phasis is based on those values.
     */
    xf86CollectInputOptions(fakeLocal, default_options, NULL);

    /* Type is mandatory */
    s = xf86FindOptionValue(fakeLocal->options, "Type");

    if (s && (xf86NameCmp(s, "stylus") == 0)) {
	local = xf86WcmAllocateStylus();
    }
    else if (s && (xf86NameCmp(s, "cursor") == 0)) {
	local = xf86WcmAllocateCursor();
    }
    else if (s && (xf86NameCmp(s, "eraser") == 0)) {
	local = xf86WcmAllocateEraser();
    }
    else {
	xf86Msg(X_ERROR, "%s: No type or invalid type specified.\n"
		"Must be one of stylus, cursor or eraser\n",
		dev->identifier);
	goto SetupProc_fail;
    }
    
    if (!local) {
	xfree(fakeLocal);
	return NULL;
    }

    priv = (WacomDevicePtr) local->private;
    common = priv->common;

    local->options = fakeLocal->options;
    local->conf_idev = fakeLocal->conf_idev;
    local->name = dev->identifier;
    xfree(fakeLocal);
    
    /* Serial Device is mandatory */
    common->wcmDevice = xf86FindOptionValue(local->options, "Device");

    if (!common->wcmDevice) {
	xf86Msg (X_ERROR, "%s: No Device specified.\n", dev->identifier);
	goto SetupProc_fail;
    }

    /* Lookup to see if there is another wacom device sharing
     * the same serial line.
     */
    localDevices = xf86FirstLocalDevice();
    
    for (; localDevices != NULL; localDevices = localDevices->next)
    {
	if (xf86WcmMatchDevice(localDevices,local))
	{
	    common = priv->common;
	    break;
	}
    }

    /* Process the common options. */
    xf86ProcessCommonOptions(local, local->options);

    /* Optional configuration */

    xf86Msg(X_CONFIG, "%s serial device is %s\n", dev->identifier,
		common->wcmDevice);

    gWacomModule.debugLevel = xf86SetIntOption(local->options, "DebugLevel", 
		gWacomModule.debugLevel);
    if (gWacomModule.debugLevel > 0) {
	xf86Msg(X_CONFIG, "WACOM: debug level set to %d\n", gWacomModule.debugLevel);
    }

    s = xf86FindOptionValue(local->options, "Mode");

    if (s && (xf86NameCmp(s, "absolute") == 0)) {
	priv->flags = priv->flags | ABSOLUTE_FLAG;
    }
    else if (s && (xf86NameCmp(s, "relative") == 0)) {
	priv->flags = priv->flags & ~ABSOLUTE_FLAG;
    }
    else if (s) {
	xf86Msg(X_ERROR, "%s: invalid Mode (should be absolute or relative). Using default.\n",
		dev->identifier);
	/* stylus/eraser defaults to absolute mode
	 * cursor defaults to relative mode
	 */
	if (priv->flags & CURSOR_ID)
	    priv->flags &= ~ABSOLUTE_FLAG;
	else
	    priv->flags |= ABSOLUTE_FLAG;
    }
    xf86Msg(X_CONFIG, "%s is in %s mode\n", local->name,
	    (priv->flags & ABSOLUTE_FLAG) ? "absolute" : "relative");

	/* ISDV4 support */
    s = xf86FindOptionValue(local->options, "ForceDevice");

    if (s && (xf86NameCmp(s, "ISDV4") == 0)) {
	common->wcmForceDevice=DEVICE_ISDV4;
	common->wcmDevCls = &gWacomISDV4Device;
	xf86Msg(X_CONFIG, "%s: forcing TabletPC ISD V4 protocol\n", dev->identifier);
    }

    common->wcmRotate=ROTATE_NONE;

    s = xf86FindOptionValue(local->options, "Rotate");

    if (s) {
	if (xf86NameCmp(s, "CW") == 0) {
	    common->wcmRotate=ROTATE_CW;
	} else if (xf86NameCmp(s, "CCW") ==0) {
	    common->wcmRotate=ROTATE_CCW;
	}
    }

    common->wcmSuppress = xf86SetIntOption(local->options, "Suppress", common->wcmSuppress);
    if ((common->wcmSuppress != 0) && /* 0 disables suppression */
		(common->wcmSuppress > MAX_SUPPRESS || 
			common->wcmSuppress < DEFAULT_SUPPRESS)) 
	common->wcmSuppress = DEFAULT_SUPPRESS;
    xf86Msg(X_CONFIG, "WACOM: suppress value is %d\n", common->wcmSuppress);      
    
    if (xf86SetBoolOption(local->options, "Tilt", (common->wcmFlags & TILT_REQUEST_FLAG))) {
	common->wcmFlags |= TILT_REQUEST_FLAG;
    }

    if (xf86SetBoolOption(local->options, "RawFilter",
                        (common->wcmFlags & RAW_FILTERING_FLAG)))
    {
	common->wcmFlags |= RAW_FILTERING_FLAG;
    }

#ifdef LINUX_INPUT
    if (xf86SetBoolOption(local->options, "USB", (common->wcmDevCls == &gWacomUSBDevice))) {
    /* best effort attempt at loading the wacom and evdev kernel modules */
    (void)xf86LoadKernelModule("wacom");
    (void)xf86LoadKernelModule("evdev");

    common->wcmDevCls = &gWacomUSBDevice;
    xf86Msg(X_CONFIG, "%s: reading USB link\n", dev->identifier);
#else
    if (xf86SetBoolOption(local->options, "USB", 0)) {
	ErrorF("The USB version of the driver isn't available for your platform\n");
#endif
    }

    /* pressure curve takes control points x1,y1,x2,y2
     * values in range from 0..100.
     * Linear curve is 0,0,100,100
     * Slightly depressed curve might be 5,0,100,95
     * Slightly raised curve might be 0,5,95,100
     */
    s = xf86FindOptionValue(local->options, "PressCurve");
    if (s) {
	int a,b,c,d;
	if ((sscanf(s,"%d,%d,%d,%d",&a,&b,&c,&d) != 4) ||
		(a < 0) || (a > 100) || (b < 0) || (b > 100) ||
		(c < 0) || (c > 100) || (d < 0) || (d > 100))
	    xf86Msg(X_CONFIG, "WACOM: PressCurve not valid\n");
	else {
	    xf86WcmSetPressureCurve(priv,a,b,c,d);
	    xf86Msg(X_CONFIG, "WACOM: PressCurve %d,%d %d,%d\n", a,b,c,d);
	}
    }

    /* Config Monitors' resoluiton in TwinView setup.
     * The value is in the form of "1024x768,1280x1024"
     * for a desktop of monitor 1 at 1024x768 and
     * monitor 2 at 1280x1024
     */
    s = xf86FindOptionValue(local->options, "TVResolution");
    if (s) {
	int a,b,c,d;
	if ((sscanf(s,"%dx%d,%dx%d",&a,&b,&c,&d) != 4) ||
		(a <= 0) || (b <= 0) || (c <= 0) || (d <= 0))
	    xf86Msg(X_CONFIG, "WACOM: TVResolution not valid\n");
	else {
	    priv->tvResolution[0] = a;
	    priv->tvResolution[1] = b;
	    priv->tvResolution[2] = c;
	    priv->tvResolution[3] = d;
 	    xf86Msg(X_CONFIG, "WACOM: TVResolution %d,%d %d,%d\n", a,b,c,d);
	}
    }

    priv->screen_no = xf86SetIntOption(local->options, "ScreenNo", -1);
    if (priv->screen_no != -1) {
	xf86Msg(X_CONFIG, "%s: attached screen number %d\n", dev->identifier,
		priv->screen_no);
    }
	
    if (xf86SetBoolOption(local->options, "KeepShape", 0)) {
	priv->flags |= KEEP_SHAPE_FLAG;
	xf86Msg(X_CONFIG, "%s: keeps shape\n", dev->identifier);
    }

    priv->topX = xf86SetIntOption(local->options, "TopX", 0);
    if (priv->topX != 0) {
	xf86Msg(X_CONFIG, "%s: top x = %d\n", dev->identifier, priv->topX);
    }
    priv->topY = xf86SetIntOption(local->options, "TopY", 0);
    if (priv->topY != 0) {
	xf86Msg(X_CONFIG, "%s: top y = %d\n", dev->identifier, priv->topY);
    }
    priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
    if (priv->bottomX != 0) {
	xf86Msg(X_CONFIG, "%s: bottom x = %d\n", dev->identifier,
		priv->bottomX);
    }
    priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
    if (priv->bottomY != 0) {
	xf86Msg(X_CONFIG, "%s: bottom y = %d\n", dev->identifier,
		priv->bottomY);
    }
    priv->serial = xf86SetIntOption(local->options, "Serial", 0);
    if (priv->serial != 0) {
	xf86Msg(X_CONFIG, "%s: serial number = %u\n", dev->identifier,
		priv->serial);
    }
    common->wcmThreshold = xf86SetIntOption(local->options, "Threshold", common->wcmThreshold);
    if (common->wcmThreshold > 0) {
	xf86Msg(X_CONFIG, "%s: threshold = %d\n", dev->identifier,
		common->wcmThreshold);
    }
    common->wcmMaxX = xf86SetIntOption(local->options, "MaxX", common->wcmMaxX);
    if (common->wcmMaxX != 0) {
	xf86Msg(X_CONFIG, "%s: max x = %d\n", dev->identifier,
		common->wcmMaxX);
    }
    common->wcmMaxY = xf86SetIntOption(local->options, "MaxY", common->wcmMaxY);
    if (common->wcmMaxY != 0) {
	xf86Msg(X_CONFIG, "%s: max x = %d\n", dev->identifier,
		common->wcmMaxY);
    }
    common->wcmMaxZ = xf86SetIntOption(local->options, "MaxZ", common->wcmMaxZ);
    if (common->wcmMaxZ != 0) {
	xf86Msg(X_CONFIG, "%s: max x = %d\n", dev->identifier,
		common->wcmMaxZ);
    }
    common->wcmUserResolX = xf86SetIntOption(local->options, "ResolutionX", common->wcmUserResolX);
    if (common->wcmUserResolX != 0) {
	xf86Msg(X_CONFIG, "%s: resol x = %d\n", dev->identifier,
		common->wcmUserResolX);
    }
    common->wcmUserResolY = xf86SetIntOption(local->options, "ResolutionY", common->wcmUserResolY);
    if (common->wcmUserResolY != 0) {
	xf86Msg(X_CONFIG, "%s: resol y = %d\n", dev->identifier,
		common->wcmUserResolY);
    }
    common->wcmUserResolZ = xf86SetIntOption(local->options, "ResolutionZ", common->wcmUserResolZ);
    if (common->wcmUserResolZ != 0) {
	xf86Msg(X_CONFIG, "%s: resol z = %d\n", dev->identifier,
		common->wcmUserResolZ);
    }
    if (xf86SetBoolOption(local->options, "ButtonsOnly", 0)) {
	priv->flags |= BUTTONS_ONLY_FLAG;
	xf86Msg(X_CONFIG, "%s: buttons only\n", dev->identifier);
    }

    for (i=0; i<16; i++)
    {
	sprintf(b, "Button%d", i+1);
	oldButton = priv->button[i];
	priv->button[i] = xf86SetIntOption(local->options, b, priv->button[i]);
	if (oldButton != priv->button[i])
	{
	    xf86Msg(X_CONFIG, "%s: button%d assigned to %d\n",
		dev->identifier, i+1, priv->button[i]);
	}
    }

    {
	int	val;
	val = xf86SetIntOption(local->options, "BaudRate", 0);

	switch(val) {
	case 38400:
	    common->wcmLinkSpeed = 38400;
	    break;
	case 19200:
	    common->wcmLinkSpeed = 19200;
	    break;
	case 9600:
	    common->wcmLinkSpeed = 9600;
	    break;
	default:
	    xf86Msg(X_ERROR, "%s: Illegal speed value (must be 9600 or 19200 or 38400).", dev->identifier);
	    break;
	}
	if (xf86Verbose)
	    xf86Msg(X_CONFIG, "%s: serial speed %u\n", dev->identifier,
		    val);
    }
    priv->speed = xf86SetRealOption(local->options, "Speed", DEFAULT_SPEED);
    if (priv->speed != DEFAULT_SPEED) {
	xf86Msg(X_CONFIG, "%s: speed = %.3f\n", dev->identifier,
		priv->speed);
    }
    priv->accel = xf86SetIntOption(local->options, "Accel", 0);
    if (priv->accel)
	xf86Msg(X_CONFIG, "%s: Accel = %d\n", dev->identifier, priv->accel);

    s = xf86FindOptionValue(local->options, "Twinview");
    if (s) xf86Msg(X_CONFIG, "%s: Twinview = %s\n", dev->identifier, s);
    if (s && xf86NameCmp(s, "none") == 0) {
	priv->twinview = TV_NONE;
	priv->dscaleX = 1.0;
	priv->dscaleY = 1.0;
	priv->doffsetX = 0;
	priv->doffsetY = 0;
    } else if (s && xf86NameCmp(s, "horizontal") == 0) {
	priv->twinview = TV_LEFT_RIGHT;
	priv->dscaleX = 2.0;
	priv->dscaleY = 1.0;
	priv->doffsetX = 0;
	priv->doffsetY = 0;
	/* default resolution */
	if(!priv->tvResolution[0]) {
	    priv->tvResolution[0] = screenInfo.screens[0]->width/2;
 	    priv->tvResolution[1] = screenInfo.screens[0]->height;
	    priv->tvResolution[2] = priv->tvResolution[0];
	    priv->tvResolution[3] = priv->tvResolution[1];
	}
    } else if (s && xf86NameCmp(s, "vertical") == 0) {
	priv->twinview = TV_ABOVE_BELOW;
	priv->dscaleX = 1.0;
	priv->dscaleY = 2.0;
	priv->doffsetX = 0;
	priv->doffsetY = 0;
	/* default resolution */
	if(!priv->tvResolution[0]) {
	    priv->tvResolution[0] = screenInfo.screens[0]->width;
	    priv->tvResolution[1] = screenInfo.screens[0]->height/2;
	    priv->tvResolution[2] = priv->tvResolution[0];
	    priv->tvResolution[3] = priv->tvResolution[1];
	}
    } else if (s) {
	xf86Msg(X_ERROR, "%s: invalid Twinview (should be none, vertical or horizontal). Using none.\n",
                        dev->identifier);
	priv->twinview = TV_NONE;
	priv->dscaleX = 1.0;
	priv->dscaleY = 1.0;
	priv->doffsetX = 0;
	priv->doffsetY = 0;
    }

    /* mark the device configured */
    local->flags |= XI86_POINTER_CAPABLE | XI86_CONFIGURED;

    /* return the LocalDevice */
    return (local);

  SetupProc_fail:
    if (common)
	xfree(common);
    if (priv)
	xfree(priv);
    if (local)
	xfree(local);
    return NULL;
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec WACOM = {
    1,				/* driver version */
    "wacom",			/* driver name */
    NULL,				/* identify */
    xf86WcmInit,		/* pre-init */
    xf86WcmUninit,		/* un-init */
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
 * xf86WcmUnplug --
 *
 * called when the module subsection is found in XF86Config
 */
static void
xf86WcmUnplug(pointer	p)
{
    DBG(1, ErrorF("xf86WcmUnplug\n"));
}

/*
 * xf86WcmPlug --
 *
 * called when the module subsection is found in XF86Config
 */
static pointer
xf86WcmPlug(pointer	module,
	    pointer	options,
	    int		*errmaj,
	    int		*errmin)
{
    /* The following message causes xf86cfg to puke. Commented out for now */
#if 0
    xf86Msg(X_INFO, "Wacom driver level: %s\n", gWacomModule.identification+strlen("$Identification: "));
#endif    
	    
    xf86AddInputDriver(&WACOM, module, 0);

    return module;
}

static XF86ModuleVersionInfo xf86WcmVersionRec =
{
    "wacom",
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

XF86ModuleData wacomModuleData = {&xf86WcmVersionRec,
				  xf86WcmPlug,
				  xf86WcmUnplug};

#endif /* XFree86LOADER */

/*
 * Local variables:
 * change-log-default-name: "~/xinput.log"
 * c-file-style: "bsd"
 * End:
 */
/* end of xf86Wacom.c */



