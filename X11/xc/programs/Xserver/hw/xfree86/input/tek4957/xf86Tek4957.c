/*
 * Copyright 2002 by Olivier DANET <odanet@caramail.com>
 *
 * Designed for XFree86 version >= 4.0
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright  notice appear  in  all  copies  and  that  both  that
 * copyright   notice   and   this  permission  notice  appear  in  supporting
 * documentation, and that the name of O. DANET may not be used in advertising
 * or  publicity pertaining to distribution  of the software without specific,
 * written  prior  permission.  O.DANET  makes  no  representations  about the
 * suitability  of  this  software  for  any  purpose. It is provided  "as is"
 * without express or implied warranty.
 *
 * O. DANET DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL O. DANET BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTIONS, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/tek4957/xf86Tek4957.c,v 1.1 2002/11/11 01:18:08 alanh Exp $ */

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
#include "exevents.h"
#include "keysym.h"
#include "mipointer.h"

#ifdef XFree86LOADER
#include "xf86Module.h"
#endif

/*
 * Debug Macros
 */

#ifdef DBG
#undef DBG
#endif
#ifdef DEBUG
#undef DEBUG
#endif

/*#define DEBUG 1*/ /* Remove comment to enable debug message */
#ifdef DEBUG
static int debug_level = 0;
#define DBG(lvl, f) {if ((lvl) <= debug_level) f;}
#else
#define DBG(lvl, f)
#endif

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))


static InputDriverPtr tekDrv;

static const char *default_options[] =
{
	"Device",       "/dev/ttyS2",
	"BaudRate",     "9600",
	"DataBits",     "7",
	"StopBits",     "1",
	"Parity",       "Odd",
	"FlowControl",  "None",
	"VTime",        "10",
	"VMin",         "1",
	NULL
};

/*
 * List of available resolutions
 */

static const int resol[] =
{
	2340,		/* 0: 1/200 inch */
	2972,		/* 1: 1/10  mm */
	11700,		/* 2: 1/1000 inch */
	11887,		/* 3: 1/40 mm */
	5850,		/* 4: 1/500 inch */
	5944,		/* 5: 1/20 mm : default */
	4680,		/* 6: 1/400 inch */
	1170,		/* 7: 1/100 inch */
	12,		/* 8: 1 inch */
	24		/* 9: 1/2 inch */
};

typedef struct
{
    char	*Device;	/* device file name */
    int		LastX;		/* last X position */
    int		LastY;		/* last Y position */
    int		LastProximity; 	/* last proximity */
    int		LastButtons;	/* last buttons state */

    int		XMax;		/* max X value */
    int		YMax;		/* max Y value */
    int		XSize;		/* active area X size */
    int		XOffset;	/* active area X offset */
    int		YSize;		/* active area Y size */
    int		YOffset;	/* active area Y offset */

    int		Resmode;	/* Resolution mode */
    int		Speed;		/* Speed */
    int	        Init;		/* Initialized ? */

    int		Index;		/* number of bytes read */
    unsigned char Data[9];	/* data read from the device */
} TekDeviceRec, *TekDevicePtr;


/*
 * TekConvert
 * Convert device valuator values to screen X and Y.
 */
static Bool
TekConvert(LocalDevicePtr	local,
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
    TekDevicePtr	priv = (TekDevicePtr) local->private;
    int W,H;
    ScreenPtr SP;

    DBG(6,xf86Msg(X_INFO,"Tek4957:TekConvert (%d,%d)\n",v0,v1));

    /* Gets current screen size. It can change dynamically in the case of
    a multi-head configuration with different screen sizes ... */
    SP=miPointerCurrentScreen();
    W=SP->width;
    H=SP->height;

    if (first != 0 || num == 1) return (FALSE);

    *x = ((long)v0 * (long)W) / (long)priv->XSize;
    *y = ((long)v1 * (long)H) / (long)priv->YSize;

    DBG(7,xf86Msg(X_INFO,"Tek4957:TekConvert ->(%d,%d)\n",*x,*y));

    return (TRUE);
}

/*
 * TekReadInput
 * Reads data and posts any new events to the server.
 */
static void
TekReadInput(LocalDevicePtr local)
{
    TekDevicePtr	priv = (TekDevicePtr) local->private;
    int			len, loop;
    int			x, y, buttons, prox;
    DeviceIntPtr	device;
    unsigned char	buffer[10];

    SYSCALL(len = xf86ReadSerial(local->fd, buffer, sizeof(buffer)));

    if (len <= 0) {
    	xf86Msg(X_ERROR,"Tek4957:Error while reading data stream\n");
	return;
    }

    for(loop=0; loop<len; loop++) {
	/* Tek4957 binary format : 8 characters
	Byte 0 :
		bit 0 : Proximity : 0=Near 1=Away
		bit 6 : 1 : Synchro
	Byte 1 :
		bit 0 : Button 0
		bit 1 : Button 1
		bit 2 : Button 2
		bit 6 : 0
	Byte 2 :
		bits 5-0 : X coord. [5:0]
		bit 6 : 0
	Byte 3 :
		bits 5-0 : X coord. [11:6]
		bit 6 : 0
	Byte 4 :
		bits 5-0 : X coord. [17:12]
		bit 6 : 0
	Byte 5 :
		bits 5-0 : Y coord. [5:0]
		bit 6 : 0
	Byte 6 :
		bits 5-0 : Y coord. [11:6]
		bit 6 : 0
	Byte 7 :
		bits 5-0 : Y coord. [17:12]
		bit 6 : 0
	*/

	if ((priv->Index == 0) && !(buffer[loop] & 0x40)) /* Check synchro bit */
             continue;

	priv->Data[priv->Index++] = buffer[loop];

	if (priv->Index == 8) {
	    priv->Index = 0;
	    prox = (priv->Data[0] & 1)? 0: 1;
	    buttons = (priv->Data[1] & 7);
	    x = (priv->Data[2]&0x3F)|((priv->Data[3]&0x3F)<<6) | ((priv->Data[4]&0x3F)<<12);
	    y = (priv->Data[5]&0x3F)|((priv->Data[6]&0x3F)<<6) | ((priv->Data[7]&0x3F)<<12);

	    x -= priv->XOffset;
	    y -= priv->YOffset;

	    if (x < 0) x = 0;
	    if (y < 0) y = 0;
	    if (x > priv->XSize) x = priv->XSize;
	    if (y > priv->YSize) y = priv->YSize;

	    device = local->dev;
	    if (prox) {
		DBG(10,xf86Msg(X_INFO,"Tek4957:TekReadInput Proximity in X=%d Y=%d Buttons=%d\n",x,y,buttons));
		if (!(priv->LastProximity))
		    xf86PostProximityEvent(device, 1, 0, 2, x, y);
		if ( (priv->LastX != x) || (priv->LastY != y) )
		    xf86PostMotionEvent(device,1, 0, 2, x, y);
		if (priv->LastButtons != buttons) {
		    if ((priv->LastButtons&1)!=(buttons&1))
			xf86PostButtonEvent(device, 1,1,((buttons&1)>0), 0, 2, x, y);
		    if ((priv->LastButtons&2)!=(buttons&2))
			xf86PostButtonEvent(device, 1,2,((buttons&2)>0), 0, 2, x, y);
		    if ((priv->LastButtons&4)!=(buttons&4))
			xf86PostButtonEvent(device, 1,3,((buttons&4)>0), 0, 2, x, y);
		}
		priv->LastButtons = buttons;
		priv->LastX = x;
		priv->LastY = y;
		priv->LastProximity = prox;
	    } else { /* Pointer away */
		DBG(10,xf86Msg(X_INFO,"Tek4957:TekReadInput Proximity out\n"));
		if (priv->LastProximity)
		    xf86PostProximityEvent(device, 0, 0, 2, x, y);
		priv->LastProximity = 0;
	    }
	}
    }

}

/*
** TekControlProc ( ??? )
*/
static void
TekControlProc(DeviceIntPtr	device, PtrCtrl *ctrl)
{
}

/*
 * TekOpen
 * Open and initialize the tablet
 */
static Bool
TekOpen(LocalDevicePtr local)
{
    char		Buffer[10];
    int			err,i;
    TekDevicePtr	priv = (TekDevicePtr)local->private;

    DBG(4,xf86Msg(X_INFO,"Tek4957:TekOpen\n"));

    /* Write ESC Z : RESET */
    SYSCALL(err = xf86WriteSerial(local->fd,"\x1B" "Z" , 2));
    if (err == -1) {
	xf86Msg(X_ERROR,"Tek4957:Write error\n");
	return !Success;
    }

    /* Wait for 100 ms */
    err = xf86WaitForInput(-1, 100000);

    /* Clear garbage, if any */
    xf86FlushInput(local->fd);

    /* Write ESC x : Ask for status */
    SYSCALL(err = xf86WriteSerial(local->fd,"\x1B" "x" , 2));
    if (err == -1) {
	xf86Msg(X_ERROR,"Tek4957:Write error\n");
	return !Success;
    }

    /* Check read data */
    i=0;
    while (i < 6) {
	err = xf86WaitForInput(local->fd, 300000);
	if (err == -1) {
	    xf86Msg(X_ERROR,"Tek4957:WaitForInput\n");
	    return !Success;
	}
	if (!err) {
	    xf86Msg(X_ERROR,"Tek4957:Timeout while reading tablet. No tablet connected ???\n");
	    return !Success;
	}
	SYSCALL(err = xf86ReadSerial(local->fd,&Buffer[i++], 1));
	if (err == -1) {
	    xf86Msg(X_ERROR,"Tek4957:Read error\n");
	    return !Success;
	}
	if (!err) break;
    }

    Buffer[i]=0;

    if ((Buffer[0] != '.') || (Buffer[1] != '#' )) {
    	xf86Msg(X_ERROR,"Tek4957:Tablet detection error %d [%s]\n",i,Buffer);
	return !Success;
    }

    /* Write ESC C [resolution]
             ESC R [speed] */
    Buffer[0]='\x1B';
    Buffer[1]='C';
    Buffer[2]='0'+priv->Resmode;
    Buffer[3]='\x1B';
    Buffer[4]='R';
    Buffer[5]='0'+priv->Speed;
    SYSCALL(err = xf86WriteSerial(local->fd,Buffer, 6));
    if (err == -1) {
	xf86Msg(X_ERROR,"Tek4957:Write error\n");
	return !Success;
    }

    /* Write ESC F 3     : Up-Left origin
             ESC I 0 0 1 : Emit data when movment
	     ESC M 0     : Emit when delta
	     */
    SYSCALL(err = xf86WriteSerial(local->fd,"\x1B""F3""\x1B""I001""\x1B""M0" , 11));
    if (err == -1) {
	 xf86Msg(X_ERROR,"Tek4957:Write error\n");
	 return !Success;
    }

    /* Flush garbage, if any */
    xf86FlushInput(local->fd);

    return Success;
}

/*
 * TekOpenDevice
 */
static Bool
TekOpenDevice(DeviceIntPtr pDev)
{
    LocalDevicePtr	local = (LocalDevicePtr)pDev->public.devicePrivate;
    TekDevicePtr	priv = (TekDevicePtr)local->private;

    local->fd = xf86OpenSerial(local->options);
    if (local->fd == -1) {
	return !Success;
    }
    xf86Msg(X_INFO,"Tek4957:%s opened as fd %d\n", priv->Device, local->fd);
    if (TekOpen(local) != Success) {
	xf86Msg(X_ERROR,"Tek4957:Initialisation error\n");
	if (local->fd >= 0) {
	    SYSCALL(xf86CloseSerial(local->fd));
	}
	local->fd = -1;
    } else {
	InitValuatorAxisStruct(pDev,
			   0,
			   0,			  /* min val */
			   priv->XSize, 	  /* max val in use */
			   20000, /* resolution dots per meter */
			   0,			  /* min_res */
			   20000); /* max_res */
    	InitValuatorAxisStruct(pDev,
			   1,
			   0,			  /* min val */
			   priv->YSize, 	  /* max val in use */
			   20000, /* resolution, dots per meter */
			   0,			  /* min_res */
			   20000); /* max_res */
	xf86Msg(X_PROBED,"Tek4957:Initialisation completed\n");
    }
    return (local->fd != -1);
}


/*
** TekProc
** Handle requests to do stuff to the driver.
*/
static int
TekProc(DeviceIntPtr pDev, int what)
{
    CARD8		map[4];
    int			loop;
    LocalDevicePtr	local = (LocalDevicePtr)pDev->public.devicePrivate;
    TekDevicePtr	priv = (TekDevicePtr)local->private;

    DBG(5,xf86Msg(X_INFO,"Tek4957:TekProc pDev=0x%x priv=0x%x what=%d\n", pDev, priv, what));

    switch (what) {
	case DEVICE_INIT:
	    DBG(2,xf86Msg(X_INFO,"Tek4957:TekProc pDev=0x%x priv=0x%x what=INIT\n", pDev, priv));
	    if (priv->Init==1) break;	/* already done */

	    for(loop=1; loop<=3; loop++) map[loop] = loop;

	    if (InitButtonClassDeviceStruct(pDev,3,map) == FALSE) {
	    	xf86Msg(X_ERROR,"Tek4957:Unable to allocate Button class device\n");
		return !Success;
	    }

	    if (InitFocusClassDeviceStruct(pDev) == FALSE) {
		xf86Msg(X_ERROR,"Tek4957:Unable to init Focus class device\n");
		return !Success;
	    }

	    if (InitPtrFeedbackClassDeviceStruct(pDev,TekControlProc) == FALSE) {
		xf86Msg(X_ERROR,"Tek4957:Unable to init ptr feedback\n");
		return !Success;
	    }

	    if (InitProximityClassDeviceStruct(pDev) == FALSE) {
		xf86Msg(X_ERROR,"Tek4957:Unable to init proximity class device\n");
		return !Success;
	    }

	    if (InitValuatorClassDeviceStruct(pDev,2,xf86GetMotionEvents,
		   local->history_size,Absolute)== FALSE) {
		xf86Msg(X_ERROR,"Tek4957:Unable to allocate Valuator class device\n");
		return !Success;
	    }
	    /* allocate the motion history buffer if needed */
	    /* xf86MotionHistoryAllocate(local); */
	    /* open the device to gather informations */
	    TekOpenDevice(pDev);
	    priv->Init=1;
	    break;

	case DEVICE_ON:
	    DBG(2,xf86Msg(X_INFO,"Tek4957:TekProc pDev=0x%x priv=0x%x what=ON\n", pDev, priv));
	    if (pDev->public.on) break;		/* already on */

	    if ((local->fd < 0) && (!TekOpenDevice(pDev))) {
		return !Success;
	    }
	    pDev->public.on = TRUE;
	    xf86AddEnabledDevice(local);
	    break;

	case DEVICE_OFF:
	    DBG(2,xf86Msg(X_INFO,"Tek4957:TekProc pDev=0x%x priv=0x%x what=OFF\n", pDev, priv));
	    if (! pDev->public.on) break;		/* already off */
	    xf86RemoveEnabledDevice(local);
	    if (local->fd >= 0)
		pDev->public.on = FALSE;
	    break;

	case DEVICE_CLOSE:
	    DBG(2,xf86Msg(X_INFO,"Tek4957:TekProc pDev=0x%x priv=0x%x what=CLOSE\n", pDev, priv));
	    if (local->fd != -1) {
	        SYSCALL(xf86CloseSerial(local->fd));
	        local->fd = -1;
	    }
	    break;

	default:
	    DBG(2,xf86Msg(X_INFO,"Tek4957:TekProc Unsupported mode=%d\n",what));
	    return !Success;
	    break;
    }
    return Success;
}

/*
 * TekClose
 */
static void
TekClose(LocalDevicePtr local)
{
    DBG(2,xf86Msg(X_INFO,"Tek4957:TekClose local = %lx, ->fd = %d\n", local, local->fd));
    if (local->fd >= 0) {
	xf86CloseSerial(local->fd);
    }
    local->fd = -1;
}

/*
** TekChangeControl
*/
static int
TekChangeControl(LocalDevicePtr local, xDeviceCtl* control)
{
    return(Success);
}

/*
** TekSwitchMode
** Switches the mode.  For now just absolute or relative, hopefully
** more on the way.
*/
static int
TekSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
    return !Success;
}

/*
 * TekUninit --
 *
 * called when the driver is unloaded.
 */
static void
TekUninit(InputDriverPtr	drv,
	  LocalDevicePtr	local,
	  int flags)
{
    TekDevicePtr	priv = (TekDevicePtr) local->private;

    ErrorF("TekUninit\n");

    TekProc(local->dev, DEVICE_OFF);

    xfree (priv);
    xf86DeleteInput(local, 0);
}

/*
 * TekInit --
 *
 * called when the module subsection is found in XF86Config
 */
static InputInfoPtr
TekInit(InputDriverPtr	drv,
	IDevPtr		dev,
	int		flags)
{
    LocalDevicePtr	local = NULL;
    TekDevicePtr	priv = NULL;
    int min,max;

    tekDrv = drv;

    xf86Msg(X_INFO,"Tek4957:Allocating device...\n");

    priv = xalloc(sizeof(TekDeviceRec));
    if (!priv) return NULL;

    local = xf86AllocateInput(tekDrv, 0);
    if (!local) {
    	xfree(priv);
	return NULL;
    }

    local->name = "TEK4957";
    local->type_name = XI_TABLET;
    local->flags = 0;
    local->device_control = TekProc;
    local->read_input = TekReadInput;
    local->control_proc = TekChangeControl;
    local->close_proc = TekClose;
    local->switch_mode = TekSwitchMode;
    local->conversion_proc = TekConvert;
    local->fd = -1;
    local->atom = 0;
    local->dev = NULL;
    local->private = priv;
    local->private_flags = 0;
    local->history_size  = 0;
    local->old_x = -1;
    local->old_y = -1;

#if defined (sun) && !defined(i386)
    char *dev_name;
#endif

#if defined(sun) && !defined(i386)
    if ((dev_name = getenv("TEK4957_DEV"))) {
	priv->Device = xalloc(strlen(dev_name) + 1);
	strcpy(priv->Device, dev_name);
	xf86Msg(X_INFO,"Tek4957:Port selected : %s\n", priv->Device);
    } else {
	priv->Device = "";
    }
#else
    priv->Device = "";         /* device file name */
#endif

    local->conf_idev = dev;

    xf86CollectInputOptions(local, default_options, NULL);
    xf86OptionListReport( local->options );

    priv = (TekDevicePtr) local->private;

    local->name = dev->identifier;


    /* Debug level */
#ifdef DEBUG
    debug_level = xf86SetIntOption(local->options, "DebugLevel", 0);
    if (debug_level > 0) {
	xf86Msg(X_CONFIG, "Tek4957:Debug level set to %d\n", debug_level);
    }
#endif

    /* Serial Device name is mandatory */
    priv->Device = xf86FindOptionValue(local->options, "Device");

    if (!priv->Device) {
	xf86Msg (X_ERROR, "Tek4957: %s: No Device specified.\n", dev->identifier);
	goto SetupProc_fail;
    }

    /* Process the common options. */
    xf86ProcessCommonOptions(local, local->options);

    /* Optional configuration */

    xf86Msg(X_CONFIG, "Tek4957: %s: serial device is %s\n", dev->identifier,
	    priv->Device);

    /* Resolution */
    priv->Resmode = xf86SetIntOption (local->options,"Resolution",5);
    priv->XMax=resol[priv->Resmode];
    priv->YMax=resol[priv->Resmode];
    if ((priv->Resmode<0)||(priv->Resmode>9)) {
	xf86Msg(X_ERROR,"Tek4957: Invalid resolution specified. Using default\n");
	priv->Resmode=5;
	priv->XMax=resol[priv->Resmode];
	priv->YMax=resol[priv->Resmode];
    } else {
    	xf86Msg(X_CONFIG,"Tek4957: Resolution [%d] = %d positions\n",priv->Resmode,priv->XMax);
    }

    /* Speed */
    priv->Speed = xf86SetIntOption (local->options, "Speed", 6 );
    if ((priv->Speed<0)||(priv->Speed>6)) {
	xf86Msg(X_ERROR,"Tek4957: Invalid speed specified. Using default\n");
	priv->Speed=5;
    } else {
    	xf86Msg(X_CONFIG,"Tek4957: Speed = %d\n",priv->Speed);
    }

    /* X bounds */
    min = xf86SetIntOption( local->options, "TopX", 0 );
    max = xf86SetIntOption( local->options, "BottomX",priv->XMax );
    if (((max-min)<=0)||(max>priv->XMax)||(min<0)) {
	xf86Msg(X_ERROR,"Tek4957:Invalid X interval specified : TopX=%d, BottomX=%d\n",min,max);
	min=0; max=priv->XMax;
    } else {
    	xf86Msg(X_CONFIG,"Tek4957:X interval :TopX=%d, BottomX=%d\n",min,max);
    }

    priv->XSize=max-min;
    priv->XOffset=min;

    /* Y bounds */
    min = xf86SetIntOption( local->options, "TopY", 0 );
    max = xf86SetIntOption( local->options, "BottomY",priv->YMax );
    if (((max-min)<=0)||(max>priv->YMax)||(min<0)) {
	xf86Msg(X_ERROR,"Tek4957:Invalid Y interval specified : TopY=%d, BottomY=%d\n",min,max);
	min=0; max=priv->XMax;
    } else {
    	xf86Msg(X_CONFIG,"Tek4957:Y interval :TopY=%d, BottomY=%d\n",min,max);
    }

    priv->YSize=max-min;
    priv->YOffset=min;

    priv->Index = 0;		/* number of bytes read */
    priv->Init = 0;
    priv->LastX = -1;		/* previous X position */
    priv->LastY = -1;		/* previous Y position */
    priv->LastProximity = 0;	/* previous proximity */
    priv->LastButtons = 0;	/* previous buttons state */

    /* return the LocalDevice */
    local->flags |= XI86_POINTER_CAPABLE | XI86_CONFIGURED;

    return local;

  SetupProc_fail:
    if (priv)
	xfree(priv);
    return local;
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec TEK4957 = {
    1,			/* driver version */
    "tek4957",		/* driver name */
    NULL,		/* identify */
    TekInit,		/* pre-init */
    TekUninit,		/* un-init */
    NULL,		/* module */
    0			/* ref count */
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
 * TekUnplug --
 *
 * called when the module subsection is found in XF86Config
 */
static void
TekUnplug(pointer p)
{
}

/*
 * TekPlug --
 *
 * called when the module subsection is found in XF86Config
 */
static pointer
TekPlug(pointer	module,
    pointer	options,
    int		*errmaj,
    int		*errmin)
{
    xf86AddInputDriver(&TEK4957, module, 0);

    return module;
}

static XF86ModuleVersionInfo TekVersionRec =
{
    "tek4957",
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

XF86ModuleData tek4957ModuleData = {&TekVersionRec,
				  TekPlug,
				  TekUnplug};

#endif /* XFree86LOADER */

/* end of xf86Tek4957.c */

