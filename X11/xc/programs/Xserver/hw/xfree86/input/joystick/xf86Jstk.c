/* $XConsortium: xf86Jstk.c /main/14 1996/10/25 14:11:36 kaleb $ */
/*
 * Copyright 1995-1999 by Frederic Lepied, France. <Lepied@XFree86.org>       
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

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/joystick/xf86Jstk.c,v 1.6 2001/12/26 21:51:59 dawes Exp $ */

#include <xf86Version.h>

#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(3,9,0,0,0)
#define XFREE86_V4 1
#endif

#ifdef XFREE86_V4
/* post 3.9 headers */

#include <misc.h>
#include <xf86.h>
#include <xf86_ansic.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <xisb.h>
#include <exevents.h>		/* Needed for InitValuator/Proximity stuff */
#include <keysym.h>

#ifdef XFree86LOADER
#include <xf86Module.h>
#endif

#define wait_for_fd(fd) xf86WaitForInput((fd), 1000)
#define tcflush(fd, n) xf86FlushInput((fd))
#undef read
#define read(a,b,c) xf86ReadSerial((a),(b),(c))
#undef write
#define write(a,b,c) xf86WriteSerial((a),(char*)(b),(c))
#define XCONFIG_PROBED "(==)"
#define XCONFIG_GIVEN "(**)"
#define xf86Verbose 1

#else /* pre 3.9 headers */

#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "XI.h"
#include "XIproto.h"

#include "compiler.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Xinput.h"
#include "xf86_OSlib.h"
#include "atKeynames.h"
#include "xf86Version.h"

#include "osdep.h"

#endif /* pre 3.9 headers */

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

/******************************************************************************
 * device records
 *****************************************************************************/

typedef struct 
{
  int           jstkFd;         /* Joystick File Descriptor */
  OsTimerPtr    jstkTimer;      /* timer object */
  int           jstkTimeout;    /* timeout to poll device */
  char          *jstkDevice;    /* device file name */
  int           jstkOldX;       /* previous X position */
  int           jstkOldY;       /* previous Y position */
  int           jstkOldButtons; /* previous buttons state */
  int           jstkMaxX;       /* max X value */
  int           jstkMaxY;       /* max Y value */
  int           jstkMinX;       /* min X value */
  int           jstkMinY;       /* min Y value */
  int           jstkCenterX;    /* X center value */
  int           jstkCenterY;    /* Y center value */
  int           jstkDelta;      /* delta cursor */
} JoystickDevRec, *JoystickDevPtr;

#ifndef XFREE86_V4
/******************************************************************************
 * configuration stuff
 *****************************************************************************/
#define DEVICENAME 1
#define TIMEOUT 2
#define MAXX 3
#define MAXY 4
#define MINX 5
#define MINY 6
#define CENTERX 7
#define CENTERY 8
#define DELTA 9
#define PORT 10
#define DEBUG_LEVEL 11
#define HISTORY_SIZE 12
#define ALWAYS_CORE 13

static SymTabRec JstkTab[] = {
  { ENDSUBSECTION,	"endsubsection" },
  { DEVICENAME,		"devicename" },
  { TIMEOUT,		"timeout" },
  { MAXX,		"maximumxposition" },
  { MAXY,		"maximumyposition" },
  { MINX,		"minimumxposition" },
  { MINY,		"minimumyposition" },
  { CENTERX,		"centerx" },
  { CENTERY,		"centery" },
  { DELTA,		"delta" },
  { PORT,		"port" },
  { DEBUG_LEVEL,	"debuglevel" },
  { HISTORY_SIZE,	"historysize" },
  { ALWAYS_CORE,        "alwayscore" },
  { -1,			"" },
};

/******************************************************************************
 * external declarations
 *****************************************************************************/

extern void xf86eqEnqueue(
    xEventPtr /*e*/
);

extern void miPointerDeltaCursor(
    int /*dx*/,
    int /*dy*/,
    unsigned long /*time*/
);

#endif /* ! XFREE86_V4 */

extern int xf86JoystickGetState(
    int   /*fd*/,
    int * /*x*/,
    int * /*y*/,
    int * /*buttons*/
    );

extern void xf86JoystickInit(void);

extern int xf86JoystickOff(
int * /*fd*/,
int /*doclose*/
);

extern int xf86JoystickOn(
char * /*name*/,
int * /*timeout*/,
int * /*centerX*/,
int * /*centerY*/
);

#ifndef XFREE86_V4
/*
 * xf86JstkConfig --
 *      Configure the device.
 */
static Bool
xf86JstkConfig(LocalDevicePtr    *array,
               int               index,
               int               max,
               LexPtr            val)
{
  LocalDevicePtr        dev = array[index];
  JoystickDevPtr        priv = (JoystickDevPtr)(dev->private);
  int token;
  
  DBG(1, ErrorF("xf86JstkConfig\n"));
      
  /* Set defaults */
  priv->jstkOldX = -1;
  priv->jstkOldY = -1;
  priv->jstkOldButtons = -1;
  priv->jstkFd = -1;
  priv->jstkTimeout = 0;

  while ((token = xf86GetToken(JstkTab)) != ENDSUBSECTION) {
    switch(token) {
    case DEVICENAME:
      if (xf86GetToken(NULL) != STRING) xf86ConfigError("Option string expected");
      dev->name = strdup(val->str);
      break;
      
    case PORT:
      if (xf86GetToken(NULL) != STRING) xf86ConfigError("Option string expected");
      priv->jstkDevice = strdup(val->str);
      break;

    case TIMEOUT:
      if (xf86GetToken(NULL) != NUMBER) xf86ConfigError("Joystick Timeout expected");
      priv->jstkTimeout = val->num;
     break;

    case MAXX:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("Joystick MaximumXPosition expected");
      priv->jstkMaxX = val->num;
     break;
      
    case MAXY:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("Joystick MaximumYPosition expected");
      priv->jstkMaxY = val->num;
     break;
      
    case MINX:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("Joystick MinimumXPosition expected");
      priv->jstkMinX = val->num;
     break;
      
    case MINY:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("Joystick MinimumYPosition expected");
      priv->jstkMinY = val->num;
     break;
      
    case CENTERX:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("Joystick CenterX expected");
      priv->jstkCenterX = val->num;
     break;
      
    case CENTERY:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("Joystick CenterY expected");
      priv->jstkCenterY = val->num;
     break;
      
    case DELTA:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("Joystick Delta expected");
      priv->jstkDelta = val->num;
     break;
      
    case DEBUG_LEVEL:
	if (xf86GetToken(NULL) != NUMBER)
	    xf86ConfigError("Option number expected");
	debug_level = val->num;
	if (xf86Verbose) {
#if DEBUG
	    ErrorF("%s Joystick debug level sets to %d\n", XCONFIG_GIVEN,
		   debug_level);      
#else
	    ErrorF("%s Joystick debug level not sets to %d because debugging is not compiled\n",
		   XCONFIG_GIVEN, debug_level);
#endif
	}
        break;

    case HISTORY_SIZE:
      if (xf86GetToken(NULL) != NUMBER)
	xf86ConfigError("Option number expected");
      dev->history_size = val->num;
      if (xf86Verbose)
	ErrorF("%s Joystick Motion history size is %d\n", XCONFIG_GIVEN,
	       dev->history_size);      
      break;
	    
    case ALWAYS_CORE:
	xf86AlwaysCore(dev, TRUE);
	if (xf86Verbose)
	    ErrorF("%s Joystick device always stays core pointer\n",
		   XCONFIG_GIVEN);
	break;

    case EOF:
      FatalError("Unexpected EOF (missing EndSubSection)");
      break; /* :-) */

    default:
      xf86ConfigError("Joystick subsection keyword expected");
      break;
    }
  }
  
  DBG(1, ErrorF("xf86JstkConfig timeout=%d name=%s maxx=%d maxy=%d minx=%d miny=%d "
		"centerx=%d centery=%d delta=%d\n",
		priv->jstkTimeout, priv->jstkDevice, priv->jstkMaxX, priv->jstkMaxY,
		priv->jstkMinX, priv->jstkMinY, priv->jstkCenterX, priv->jstkCenterY,
		priv->jstkDelta));

  if (xf86Verbose) {
    ErrorF("%s %s: timeout=%d port=%s maxx=%d maxy=%d minx=%d miny=%d\n"
	   "\tcenterx=%d centery=%d delta=%d\n", XCONFIG_GIVEN, dev->name,
	   priv->jstkTimeout, priv->jstkDevice, priv->jstkMaxX, priv->jstkMaxY,
	   priv->jstkMinX, priv->jstkMinY, priv->jstkCenterX, priv->jstkCenterY,
	   priv->jstkDelta);
  }
  return Success;
}
#endif

/*
 ***************************************************************************
 *
 * xf86JstkConvert --
 *	Convert valuators to X and Y.
 *
 ***************************************************************************
 */
static Bool
xf86JstkConvert(LocalDevicePtr	local,
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
    if (first != 0 || num != 2)
      return FALSE;

    *x = v0;
    *y = v1;

    return TRUE;
}

/*
 * xf86JstkEvents --
 *      Read the new events from the device, and enqueue them.
 */
static CARD32
xf86JstkEvents(OsTimerPtr        timer,
               CARD32            atime,
               pointer           arg)
{
  DeviceIntPtr          device = (DeviceIntPtr)arg;
  JoystickDevPtr        priv = (JoystickDevPtr) PRIVATE(device);
  int                   timeout = priv->jstkTimeout;
  int                   x, y, buttons;

  DBG(5, ErrorF("xf86JstkEvents BEGIN device=0x%x priv=0x%x"
                " timeout=%d timer=0x%x\n",
                device, priv, timeout, priv->jstkTimer));
  
  if (xf86JoystickGetState(priv->jstkFd, &x, &y, &buttons)) {
    int loop;
    int length = priv->jstkMaxX - priv->jstkMinX;
    int height = priv->jstkMaxY - priv->jstkMinY;
    int v0 = ((x - priv->jstkMinX) * priv->jstkDelta) / length -
      (priv->jstkDelta / 2);
    int v1 = ((y - priv->jstkMinY) * priv->jstkDelta) / height - 
      (priv->jstkDelta / 2);

    DBG(4, ErrorF("xf86JoystickGetState x=%d y=%d centerX=%d centerY=%d v0=%d "
                  "v1=%d buttons=%d\n",
                  x, y, priv->jstkCenterX, priv->jstkCenterY,
                  v0, v1, buttons));
    
    if ((abs(v0) > (priv->jstkDelta / 20)) ||
        (abs(v1) > (priv->jstkDelta / 20)))
      {
	xf86PostMotionEvent(device, 0, 0, 2, v0, v1);
	
        priv->jstkOldX = x;
        priv->jstkOldY = y;          
      }
    for(loop=1; loop<3; loop++)
      {
        if ((priv->jstkOldButtons & loop) != (buttons & loop))
          {
	    xf86PostButtonEvent(device, 0, loop, ((buttons & loop) == loop),
				0, 2, v0, v1);
          }
      }
    priv->jstkOldButtons = buttons;
  }

  DBG(3, ErrorF("xf86JstkEvents END   device=0x%x priv=0x%x"
                " timeout=%d timer=0x%x\n",
                device, priv, timeout, priv->jstkTimer));

  return timeout;
}

static void
xf86JstkControlProc(DeviceIntPtr	device,
                    PtrCtrl		*ctrl)
{
  DBG(2, ErrorF("xf86JstkControlProc\n"));
}

/*
 * xf86JstkProc --
 *      Handle the initialization, etc. of a joystick
 */
static int
xf86JstkProc(DeviceIntPtr       pJstk,
	     int                what)
{
  CARD8                 map[5];
  int                   nbaxes;
  int                   nbbuttons;
  int                   jstkfd;
  LocalDevicePtr        local = (LocalDevicePtr)pJstk->public.devicePrivate;
  JoystickDevPtr        priv = (JoystickDevPtr)PRIVATE(pJstk);

  DBG(2, ErrorF("BEGIN xf86JstkProc dev=0x%x priv=0x%x xf86JstkEvents=0x%x\n",
                pJstk, priv, xf86JstkEvents));
  
  switch (what)
    {
    case DEVICE_INIT: 
      DBG(1, ErrorF("xf86JstkProc pJstk=0x%x what=INIT\n", pJstk));
  
      map[1] = 1;
      map[2] = 2;

      nbaxes = 2;
      nbbuttons = 2;

      if (InitButtonClassDeviceStruct(pJstk,
                                      nbbuttons,
                                      map) == FALSE) 
        {
          ErrorF("unable to allocate Button class device\n");
          return !Success;
        }
      
      if (InitFocusClassDeviceStruct(pJstk) == FALSE)
        {
          ErrorF("unable to init Focus class device\n");
          return !Success;
        }
          
      if (InitPtrFeedbackClassDeviceStruct(pJstk,
                                           xf86JstkControlProc) == FALSE)
        {
          ErrorF("unable to init ptr feedback\n");
          return !Success;
        }
          
      if (InitValuatorClassDeviceStruct(pJstk, 
                                    nbaxes,
                                    xf86GetMotionEvents, 
                                    local->history_size,
                                    Relative) /* relatif ou absolute */
          == FALSE) 
        {
          ErrorF("unable to allocate Valuator class device\n"); 
          return !Success;
        }
      else 
        {
	    InitValuatorAxisStruct(pJstk,
				   0,
				   0, /* min val */
				   screenInfo.screens[0]->width, /* max val */
				   1, /* resolution */
				   0, /* min_res */
				   1); /* max_res */
	    InitValuatorAxisStruct(pJstk,
				   1,
				   0, /* min val */
				   screenInfo.screens[0]->height, /* max val */
				   1, /* resolution */
				   0, /* min_res */
				   1); /* max_res */
	    
	  /* allocate the motion history buffer if needed */
	  xf86MotionHistoryAllocate(local);

          xf86JoystickInit();
#ifndef XFREE86_V4
          AssignTypeAndName(pJstk, local->atom, local->name);
#endif
        }

      break; 
      
    case DEVICE_ON:
      priv->jstkFd = jstkfd = xf86JoystickOn(priv->jstkDevice,
                                             &(priv->jstkTimeout),
					     &(priv->jstkCenterX),
					     &(priv->jstkCenterY));

      DBG(1, ErrorF("xf86JstkProc  pJstk=0x%x what=ON name=%s\n", pJstk,
                    priv->jstkDevice));

      if (jstkfd != -1)
      {
        priv->jstkTimer = TimerSet(NULL, 0, /*TimerAbsolute,*/
                                   priv->jstkTimeout,
                                   xf86JstkEvents,
                                   (pointer)pJstk);
        pJstk->public.on = TRUE;
        DBG(2, ErrorF("priv->jstkTimer=0x%x\n", priv->jstkTimer));
      }
      else
        return !Success;
    break;
      
    case DEVICE_OFF:
    case DEVICE_CLOSE:
      DBG(1, ErrorF("xf86JstkProc  pJstk=0x%x what=%s\n", pJstk,
                    (what == DEVICE_CLOSE) ? "CLOSE" : "OFF"));

      jstkfd = xf86JoystickOff(&(priv->jstkFd), (what == DEVICE_CLOSE));
      pJstk->public.on = FALSE;
    break;

    default:
      ErrorF("unsupported mode=%d\n", what);
      return !Success;
      break;
    }
  DBG(2, ErrorF("END   xf86JstkProc dev=0x%x priv=0x%x xf86JstkEvents=0x%x\n",
                pJstk, priv, xf86JstkEvents));
  return Success;
}

/*
 * xf86JstkAllocate --
 *      Allocate Joystick device structures.
 */
static LocalDevicePtr
xf86JstkAllocate(void)
{
  LocalDevicePtr        local = xalloc(sizeof(LocalDeviceRec));
  JoystickDevPtr        priv = xalloc(sizeof(JoystickDevRec));
  
  local->name = "JOYSTICK";
  local->flags = 0;
#ifndef XFREE86_V4
  local->device_config = xf86JstkConfig;
#endif
  local->device_control = xf86JstkProc;
  local->read_input = NULL;
  local->close_proc = NULL;
  local->control_proc = NULL;
  local->switch_mode = NULL;
  local->conversion_proc = xf86JstkConvert;
  local->fd = -1;
  local->atom = 0;
  local->dev = NULL;
  local->private = priv;
  local->type_name = "Joystick";
  local->history_size  = 0;
  
  priv->jstkFd = -1;
  priv->jstkTimer = NULL;
  priv->jstkTimeout = 0;
  priv->jstkDevice = NULL;
  priv->jstkOldX = -1;
  priv->jstkOldY = -1;
  priv->jstkOldButtons = -1;
  priv->jstkMaxX = 1000;
  priv->jstkMaxY = 1000;
  priv->jstkMinX = 0;
  priv->jstkMinY = 0;
  priv->jstkCenterX = -1;
  priv->jstkCenterY = -1;
  priv->jstkDelta = 100;
  
  return local;
}

/*
 * joystick association
 */
DeviceAssocRec joystick_assoc =
{
  "joystick",                   /* config_section_name */
  xf86JstkAllocate              /* device_allocate */
};

#ifndef XFREE86_V4

#ifdef DYNAMIC_MODULE
/*
 * entry point of dynamic loading
 */
int
#ifndef DLSYM_BUG
init_module(unsigned long	server_version)
#else
init_xf86Jstk(unsigned long     server_version)
#endif
{
    xf86AddDeviceAssoc(&joystick_assoc);

    if (server_version != XF86_VERSION_CURRENT) {
	ErrorF("Warning: Joystick module compiled for version%s\n", XF86_VERSION);
	return 0;
    } else {
	return 1;
    }
}
#endif

#else /* ! XFREE86_V4 */

/*
 ***************************************************************************
 *
 * Dynamic loading functions
 *
 ***************************************************************************
 */
#ifdef XFree86LOADER

/*
 * xf86JstckUnplug --
 *
 * called when the module subsection is found in XF86Config
 */
static void
xf86JstkUnplug(pointer	p)
{
    LocalDevicePtr local = (LocalDevicePtr) p;
    JoystickDevPtr priv = (JoystickDevPtr) local->private;
    
    ErrorF("xf86JstckUnplug\n");
    
    xf86JstkProc(local->dev, DEVICE_OFF);
    
    xfree (priv);
    xfree (local);
}

/*
 * xf86JstckPlug --
 *
 * called when the module subsection is found in XF86Config
 */
static pointer
xf86JstkPlug(pointer	module,
	     pointer	options,
	     int	*errmaj,
	     int	*errmin )
{
    LocalDevicePtr	local = NULL;
    JoystickDevPtr	priv = NULL;
    char		*s;

    local = xf86JstkAllocate();

    if (!local || !priv) {
	*errmaj = LDR_NOMEM;
	goto SetupProc_fail;
    }

    priv = (JoystickDevPtr) local->private;

    /* Joytsick device is mandatory */
    priv->jstkDevice = xf86FindOptionValue(options, "Device");

    if (!priv->jstkDevice) {
	xf86Msg (X_ERROR, "WACOM: No Device specified.\n");
	*errmaj = LDR_BADUSAGE;
	goto SetupProc_fail;
    }

    /* Optional configuration */

    s = xf86SetStrOption(options, "DeviceName", NULL);
    if (s != NULL)
	local->name = s;

    xf86Msg(X_CONFIG, "%s name is %s\n", local->type_name, local->name);
    xf86Msg(X_CONFIG, "JOYSTICK device is %s\n", priv->jstkDevice);

    debug_level = xf86SetIntOption(options, "DebugLevel", 0);
    if (debug_level > 0) {
	xf86Msg(X_CONFIG, "JOYSTICK: debug level set to %d\n", debug_level);
    }

    priv->jstkMaxX = xf86SetIntOption(options, "MaxX", 1000);
    if (priv->jstkMaxX != 1000) {
	xf86Msg(X_CONFIG, "JOYSTICK: max x = %d\n", priv->jstkMaxX);
    }
    priv->jstkMaxY = xf86SetIntOption(options, "MaxY", 1000);
    if (priv->jstkMaxY != 1000) {
	xf86Msg(X_CONFIG, "JOYSTICK: max y = %d\n", priv->jstkMaxY);
    }
    priv->jstkMinX = xf86SetIntOption(options, "MinX", 0);
    if (priv->jstkMinX != 0) {
	xf86Msg(X_CONFIG, "JOYSTICK: min x = %d\n", priv->jstkMinX);
    }
    priv->jstkMinY = xf86SetIntOption(options, "MinY", 0);
    if (priv->jstkMinY != 0) {
	xf86Msg(X_CONFIG, "JOYSTICK: min y = %d\n", priv->jstkMinY);
    }
	    
    priv->jstkCenterX = xf86SetIntOption(options, "CenterX", -1);
    if (priv->jstkCenterX != -1) {
	xf86Msg(X_CONFIG, "JOYSTICK: center x = %d\n", priv->jstkCenterX);
    }
    priv->jstkCenterY = xf86SetIntOption(options, "CenterY", -1);
    if (priv->jstkCenterY != 0) {
	xf86Msg(X_CONFIG, "JOYSTICK: center y = %d\n", priv->jstkCenterY);
    }

    priv->jstkTimeout = xf86SetIntOption(options, "Timeout", -1);
    if (priv->jstkTimeout != -1) {
	xf86Msg(X_CONFIG, "JOYSTICK: timeout = %d\n", priv->jstkTimeout);
    }

    priv->jstkDelta = xf86SetIntOption(options, "Delta", 0);
    if (priv->jstkDelta != 0) {
	xf86Msg(X_CONFIG, "JOYSTICK: delta = %d\n", priv->jstkDelta);
    }

    /* Register the device into XFree86 XInput layer */
    xf86AddLocalDevice(local, options);

    /* return the LocalDevice */
    return (local);

  SetupProc_fail:
    if (priv)
	xfree(priv);
    if (local)
	xfree(local);
    return NULL;
}

static XF86ModuleVersionInfo xf86JstkVersionRec =
{
	"joystick",
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

XF86ModuleData joystickModuleData = {&xf86JstkVersionRec,
					 xf86JstkPlug,
					 xf86JstkUnplug};
#endif /* XFree86LOADER */

#endif /* ! XFREE86_V4 */

	
/* end of xf86Jstk.c */
