/*
 * Copyright 2002 by Brian Goines (bgoines78@comcast.net)
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Brian Goines not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Brian Goines  makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * BRIAN GOINES DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/jamstudio/js_x.c,v 1.2 2002/11/15 16:05:25 tsi Exp $ */

#include <sys/types.h>
#include "xf86Version.h"
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(3,9,0,0,0)
#define XFREE86_V4 1
#endif
#include "misc.h"
#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "xf86Xinput.h"
#include "exevents.h"		/* Needed for InitValuator/Proximity stuff */
#include "mipointer.h"

#ifdef XFree86LOADER
#include "xf86Module.h"
#endif

#define JSX_XCOORD	65584
#define JSX_YCOORD	65585
#define JSX_PRESS		852016
#define JSX_BTN		852034

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

#ifdef XFREE86_V4

struct hiddev_event
{
   unsigned hid;
   signed int value;
};

typedef struct
{
   int jsxFd;
   int jsxTimeout;
   char *jsxDevice;
   int jsxOldX;
   int jsxOldY;
   int jsxOldPress;
   int jsxOldBtn;
   int jsxOldNotify;
   int jsxMaxX;
   int jsxMaxY;
   int jsxMinX;
   int jsxMinY;
   int jsxPressMax;
   int jsxPressMin;
   int jsxPressDiv;
}
JS_XDevRec, *JS_XDevPtr;

static void
xf86JS_XReadInput(LocalDevicePtr local)
{
   JS_XDevPtr priv = local->private;
   struct hiddev_event event;
   int x = priv->jsxOldX, y = priv->jsxOldY, press = priv->jsxOldPress;
   int btn = priv->jsxOldBtn, len = 0;
   int btn_notify = priv->jsxOldNotify;

#if 0
   SYSCALL(len = read(local->fd, &event, sizeof(struct hiddev_event)));
   if (len < sizeof(struct hiddev_event))
      return;
#endif
   while ((len = read(local->fd, &event, sizeof(struct hiddev_event)))
	  == sizeof(struct hiddev_event)) {
      switch (event.hid) {
      case JSX_XCOORD:
	 x = event.value;
	 break;
      case JSX_YCOORD:
	 y = event.value;
	 break;
      case JSX_PRESS:
	 press = event.value / priv->jsxPressDiv;
	 break;
      case JSX_BTN:
	 priv->jsxOldBtn = btn = event.value;
	 break;
      }
   }
   x = x > 0 ? x : 0;
   x = x < priv->jsxMaxX ? x : priv->jsxMaxX;
   y = y > 0 ? y : 0;
   y = y < priv->jsxMaxY ? y : priv->jsxMaxY;
   press = press > 0 ? press : 0;
   press = press < priv->jsxPressMax ? press : priv->jsxPressMax;

   if ((press > priv->jsxPressMin) && (btn == 1))
      btn_notify = 1;
   else
      btn_notify = 0;

   if ((x != priv->jsxOldX) || (y != priv->jsxOldY)
       || (press != priv->jsxOldPress)) {
      xf86PostMotionEvent(local->dev, 1, 0, 3, x, y, press);
      priv->jsxOldX = x;
      priv->jsxOldY = y;
      priv->jsxOldPress = press;
   }
   if (btn_notify != priv->jsxOldNotify) {
      xf86PostButtonEvent(local->dev, 0, 1, btn_notify, 0, 3, x, y, press);
      priv->jsxOldNotify = btn_notify;
   }
}

static int
xf86JS_XConnect(DeviceIntPtr pJS_X)
{
   LocalDevicePtr local = (LocalDevicePtr) pJS_X->public.devicePrivate;
   JS_XDevPtr priv = local->private;

   local->fd = xf86OpenSerial(local->options);
   InitValuatorAxisStruct(pJS_X, 0, priv->jsxMinX, priv->jsxMaxX,
			  priv->jsxMaxX / 7.5, 0, priv->jsxMaxX / 7.5);
   InitValuatorAxisStruct(pJS_X, 1, priv->jsxMinY, priv->jsxMaxY,
			  priv->jsxMaxY / 5.5, 0, priv->jsxMaxY / 5.5);
   InitValuatorAxisStruct(pJS_X, 2, priv->jsxPressMin, priv->jsxPressMax,
			  128, 0, 128);
   return (local->fd > 0);
}

static Bool
xf86JS_XConvert(LocalDevicePtr local, int first, int num, int v0, int v1,
		int v2, int v3, int v4, int v5, int *x, int *y)
{
   JS_XDevPtr priv = local->private;
   int width, height;
   int deltaX, deltaY;

   width = miPointerCurrentScreen()->width;
   height = miPointerCurrentScreen()->height;
/*
deltaX=(float)width/priv->jsxMaxX; deltaY=(float)height/priv->jsxMaxY;
*/
   deltaX = priv->jsxMaxX / width;
   deltaY = priv->jsxMaxY / height;
   *x = v0 / deltaX;
   *y = v1 / deltaY;
   xf86XInputSetScreen(local, 0, *x, *y);
   return TRUE;
}

static void
xf86JS_XControlProc(DeviceIntPtr device, PtrCtrl * ctrl)
{
   return;
}

static int
xf86JS_XProc(DeviceIntPtr pJS_X, int operation)
{
   LocalDevicePtr local = (LocalDevicePtr) pJS_X->public.devicePrivate;
   int nbaxes = 3;			/* X Y Pressure */
   int nbuttons = 1;			/* This this is necessary for most apps to work. */
   CARD8 map[2] = { 0, 1 };

   switch (operation) {
   case DEVICE_INIT:
      if (InitButtonClassDeviceStruct(pJS_X, nbuttons, map) == FALSE)
	 return !Success;
      if (InitFocusClassDeviceStruct(pJS_X) == FALSE)
	 return !Success;
      if (InitPtrFeedbackClassDeviceStruct(pJS_X, xf86JS_XControlProc) ==
	  FALSE)
	 return !Success;
      if (InitProximityClassDeviceStruct(pJS_X) == FALSE)
	 return !Success;
      if (InitValuatorClassDeviceStruct(pJS_X, nbaxes, xf86GetMotionEvents,
					local->history_size,
					Absolute | OutOfProximity) == FALSE)
	 return !Success;
      else
	 xf86MotionHistoryAllocate(local);
      xf86JS_XConnect(pJS_X);
      break;
   case DEVICE_ON:
      if (local->fd == -1)
	 xf86JS_XConnect(pJS_X);
      xf86AddEnabledDevice(local);
      pJS_X->public.on = TRUE;
      break;
   case DEVICE_OFF:
      if (local->fd > 0)
	 xf86RemoveEnabledDevice(local);
   case DEVICE_CLOSE:
      if (local->fd > 0) {
	 SYSCALL(close(local->fd));
	 local->fd = -1;
      }
      break;
   default:
      xf86Msg(X_ERROR, "JamStudio: Unhandled operation number %d.\n",
	      operation);
      break;
   }
   return Success;
}

static int
xf86JS_XChangeControl(LocalDevicePtr local, xDeviceCtl * control)
{
   return Success;
}

static int
xf86JS_XSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
   return Success;
}

static LocalDevicePtr
xf86JS_XAllocate(InputDriverPtr drv)
{
   LocalDevicePtr local = xf86AllocateInput(drv, 0);
   JS_XDevPtr priv = xalloc(sizeof(JS_XDevRec));

   memset(priv, 0, sizeof(JS_XDevRec));
   local->name = "JAMSTUDIO";
   local->flags = 0;
   local->device_control = xf86JS_XProc;
   local->read_input = xf86JS_XReadInput;
   local->close_proc = NULL;
   local->control_proc = xf86JS_XChangeControl;
   local->switch_mode = xf86JS_XSwitchMode;
   local->conversion_proc = xf86JS_XConvert;
   local->fd = -1;
   local->atom = 0;
   local->dev = NULL;
   local->private = priv;
   local->type_name = "JamStudio";
   local->history_size = 0;
   local->old_x = local->old_y = -1;

   priv->jsxFd = -1;
   priv->jsxTimeout = 0;
   priv->jsxDevice = NULL;
   priv->jsxOldX = -1;
   priv->jsxOldY = -1;
   priv->jsxOldPress = priv->jsxOldBtn = priv->jsxOldNotify = -1;
   priv->jsxMaxX = 8000;
   priv->jsxMaxY = 6000;
   priv->jsxMinX = 0;
   priv->jsxMinY = 0;
   priv->jsxPressMin = 5;
   priv->jsxPressMax = 127;
   priv->jsxPressDiv = 2;

   return local;
}

static void
xf86JS_XUnInit(InputDriverPtr drv, LocalDevicePtr local, int flags)
{
   JS_XDevPtr priv = local->private;

   xf86JS_XProc(local->dev, DEVICE_CLOSE);
   xfree(priv);
   xf86DeleteInput(local, 0);
}

static InputInfoPtr
xf86JS_XInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
   LocalDevicePtr local = NULL;
   JS_XDevPtr priv = NULL;
   pointer options;

   if ((local = xf86JS_XAllocate(drv)) == NULL) {
      xf86Msg(X_ERROR, "Could not allocate local device.\n");
      return NULL;
   }
   if (local->private == NULL) {
      xf86Msg(X_ERROR, "Could not allocate private structure.\n");
      xfree(local);
      return NULL;
   }
   local->conf_idev = dev;
   xf86CollectInputOptions(local, NULL, NULL);
   options = local->options;
   local->name = dev->identifier;
   priv = (JS_XDevPtr) local->private;
   priv->jsxDevice = xf86FindOptionValue(options, "Device");
   xf86ProcessCommonOptions(local, local->options);
   if (!priv->jsxDevice) {
      xf86Msg(X_ERROR, "JamStudio: No Device specified.\n");
      return NULL;
   }
   priv->jsxMaxX = xf86SetIntOption(options, "MaxX", 8000);
   priv->jsxMaxY = xf86SetIntOption(options, "MaxY", 6000);
   priv->jsxMinX = xf86SetIntOption(options, "MinX", 0);
   priv->jsxMinY = xf86SetIntOption(options, "MinY", 0);
   priv->jsxPressMax = xf86SetIntOption(options, "PressMax", 127);
   priv->jsxPressMin = xf86SetIntOption(options, "PressMin", 5);
   priv->jsxPressDiv = xf86SetIntOption(options, "PressDiv", 2);
   local->flags |= XI86_POINTER_CAPABLE | XI86_CONFIGURED;
   return (local);
}

InputDriverRec JAMSTUDIO =
      { 1, "js_x", NULL, xf86JS_XInit, xf86JS_XUnInit, NULL, 0 };

#ifdef XFree86LOADER

static void
xf86JS_XUnplug(pointer p)
{
   return;
}

static pointer
xf86JS_XPlug(pointer module, pointer options, int *errmaj, int *errmin)
{
   xf86AddInputDriver(&JAMSTUDIO, module, 0);
   return module;
}

static XF86ModuleVersionInfo xf86JS_XVersionRec = {
   "js_x",
   MODULEVENDORSTRING,
   MODINFOSTRING1,
   MODINFOSTRING2,
   XF86_VERSION_CURRENT,
   1, 0, 0,
   ABI_CLASS_XINPUT,
   ABI_XINPUT_VERSION,
   MOD_CLASS_XINPUT,
   {0, 0, 0, 0}		/* signature, to be patched into the file by a tool */
};

XF86ModuleData js_xModuleData = { &xf86JS_XVersionRec,
   xf86JS_XPlug,
   xf86JS_XUnplug
};
#endif
#endif
