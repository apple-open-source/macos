/* 
 * Copyright (c) 1998  Metro Link Incorporated
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, cpy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the Metro Link shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Metro Link.
 *
 */

/*
 * port to XFree4.2.0 Copyright (c) 2002 Christoph Koulen
 * chris@real-aix.de
 *
 * port based on pre-XFree4.2.0 driver code v1.10 and XFree4.2.0 SpaceOrb driver code
 */
 
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/magellan/magellan.c,v 1.11 2002/06/07 21:03:27 alanh Exp $ */

#define _MAGELLAN_C_
/*****************************************************************************
 *	Standard Headers
 ****************************************************************************/

#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#include <xf86_ansic.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <xisb.h>
#include <exevents.h>			/* Needed for InitValuator/Proximity stuff	*/

/*****************************************************************************
 *	Local Headers
 ****************************************************************************/
#include "magellan.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables
 ****************************************************************************/

InputDriverRec MAGELLAN = {
       1,
       "magellan",
       NULL,
       MagellanPreInit,
       NULL,
       NULL,
       0
};

#ifdef XFree86LOADER

static XF86ModuleVersionInfo VersionRec =
{
	"magellan",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	1, 0, 0,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{0, 0, 0, 0}				/* signature, to be patched into the file by
								 * a tool */
};

/* The following list of symbols that has been taken without modification from the 
   SpaceOrb driver code.
   This has resulted in a working magellan driver.
   You may be able to strip this list for the magellan device;
 */
static const char *reqSymbols[] = {
        "AddEnabledDevice",
        "ErrorF",
        "InitButtonClassDeviceStruct",
        "InitProximityClassDeviceStruct",
        "InitValuatorAxisStruct",
        "InitValuatorClassDeviceStruct",
        "InitPtrFeedbackClassDeviceStruct",
        "RemoveEnabledDevice",
        "Xcalloc",
        "Xfree",
        "XisbBlockDuration",
        "XisbFree",
        "XisbNew",
        "XisbRead",
        "XisbTrace",
        "screenInfo",
        "xf86AddInputDriver",
        "xf86AllocateInput",
        "xf86CloseSerial",
        "xf86CollectInputOptions",
        "xf86ErrorFVerb",
        "xf86FindOptionValue",
        "xf86GetMotionEvents",
        "xf86GetVerbosity",
        "xf86MotionHistoryAllocate",
        "xf86NameCmp",
        "xf86OpenSerial",
        "xf86OptionListCreate",
        "xf86OptionListMerge",
        "xf86OptionListReport",
        "xf86PostButtonEvent",
        "xf86PostMotionEvent",
        "xf86PostProximityEvent",
        "xf86ProcessCommonOptions",
        "xf86ScaleAxis",
        "xf86SetIntOption",
        "xf86SetStrOption",
        "xf86XInputSetScreen",
        "xf86XInputSetSendCoreEvents",
        NULL
};

static pointer
MAGELLANSetupProc(pointer module,
		  pointer options,
		  int *errmaj,
		  int *errmin ) {
  xf86LoaderReqSymLists(reqSymbols, NULL);
  xf86AddInputDriver(&MAGELLAN, module, 0);
  return (pointer) 1;
}

/* 
 * The TearDownProc may have to be tailored to your device
 */
static void
TearDownProc( pointer p )
{
  if (!xf86ServerIsOnlyDetecting()) {
  
	InputInfoPtr pInfo = (InputInfoPtr) p;
	MAGELLANPrivatePtr priv = (MAGELLANPrivatePtr) pInfo->private;

	DeviceOff (pInfo->dev);

	xf86CloseSerial (pInfo->fd);
	XisbFree (priv->buffer);
	xfree (priv);
	xfree (pInfo->name);
	xfree (pInfo);
  }
}

XF86ModuleData magellanModuleData = { &VersionRec, MAGELLANSetupProc, TearDownProc };

#endif /* XFreeLOADER */
/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning;
 *
 * These settings have been found to work for a device which identifies itself as follows:
 *
 * MAGELLAN  Version 5.49  by LOGITECH INC. 10/22/96
 *
 * newer devices may require different settings.
 */
static const char *default_options[] =
{
	"BaudRate", "9600",
	"StopBits", "2",
	"DataBits", "8",
	"Parity", "None",
	"Vmin", "26",
	"Vtime", "1",
	"FlowControl", "Xoff",
        NULL,
};


/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static InputInfoPtr
MagellanPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{              
        InputInfoPtr pInfo;
        MAGELLANPrivatePtr priv = xcalloc (1, sizeof (MAGELLANPrivateRec));

	if (!priv)
		return NULL;

        if (!(pInfo = xf86AllocateInput(drv, 0))) {
		xfree(priv);
                return NULL;
	}
  
	priv->lex_mode = magellan_normal;
	priv->packeti = 0;
	priv->old_buttons=0;
	priv->buffer= NULL;
	
	pInfo->type_name = XI_SPACEBALL;
	pInfo->device_control = DeviceControl;
	pInfo->read_input = ReadInput;
	pInfo->control_proc = ControlProc;
	pInfo->close_proc = CloseProc;
	pInfo->switch_mode = SwitchMode;
	pInfo->conversion_proc = ConvertProc;
	pInfo->dev = NULL;
	pInfo->private = priv;
	pInfo->private_flags = 0;
	pInfo->flags = 0;
        pInfo->conf_idev = dev;

	xf86CollectInputOptions(pInfo, default_options, NULL);

        xf86OptionListReport( pInfo->options );

        pInfo->fd = xf86OpenSerial (pInfo->options);
        if (pInfo->fd == -1)
        {
	  ErrorF ("MAGELLAN driver unable to open device\n");
	  goto SetupProc_fail;
        }
	
	priv->buffer = XisbNew (pInfo->fd, 200);
	DBG (9, XisbTrace (priv->buffer, 1));

	/* 
	 * Verify the hardware is attached and functional
	 */
        if (QueryHardware (priv) != Success)
        {
	  ErrorF ("Unable to query/initialize MAGELLAN hardware.\n");
	  goto SetupProc_fail;
        }

        /* this results in an xstrdup that must be freed later */
	/* the last string "spaceball" is the one to make glut 3.7 believe
	   a "spaceball" device ist present. 
	   This decision is based on string comparison!
	*/
        pInfo->name = xf86SetStrOption( pInfo->options, "DeviceName", "spaceball");
	pInfo->history_size = xf86SetIntOption( pInfo->options, "HistorySize", 0 );
	
        xf86ProcessCommonOptions(pInfo, pInfo->options);
	
        pInfo->flags |= XI86_CONFIGURED;
        return (pInfo);

	/* 
	 * If something went wrong, cleanup and return NULL
	 */
  SetupProc_fail:
        if ((pInfo) && (pInfo->fd))
                xf86CloseSerial (pInfo->fd);
        if ((pInfo) && (pInfo->name))
                xfree (pInfo->name);

        if ((priv) && (priv->buffer))
                XisbFree (priv->buffer);
	if (priv)
                xfree (priv);
        return (pInfo);
}



static Bool
DeviceControl (DeviceIntPtr dev, int mode)
{
	Bool	RetValue;

	switch (mode)
	{
	case DEVICE_INIT:
		DeviceInit (dev);
		RetValue = Success;
		break;
	case DEVICE_ON:
		RetValue = DeviceOn( dev );
		break;
	case DEVICE_OFF:
		RetValue = DeviceOff( dev );
		break;
	case DEVICE_CLOSE:
		RetValue = DeviceClose( dev );
		break;
	default:
		RetValue = BadValue;
	}

	return( RetValue );
}

static Bool
DeviceOn (DeviceIntPtr dev)
{
	InputInfoPtr pInfo = dev->public.devicePrivate;

	AddEnabledDevice (pInfo->fd);
	dev->public.on = TRUE;
	return (Success);
}

static Bool
DeviceOff (DeviceIntPtr dev)
{
	InputInfoPtr pInfo = dev->public.devicePrivate;

	RemoveEnabledDevice (pInfo->fd);
	dev->public.on = FALSE;
	return (Success);
}

static Bool
DeviceClose (DeviceIntPtr dev)
{
	return (Success);
}

static Bool
DeviceInit (DeviceIntPtr dev)
{
  InputInfoPtr pInfo = dev->public.devicePrivate;
  unsigned char map[] =
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  int i;

  if (InitButtonClassDeviceStruct (dev, 9, map) == FALSE)
    {
      ErrorF ("Unable to allocate MAGELLAN ButtonClassDeviceStruct\n");
      return !Success;
    }

  if (InitFocusClassDeviceStruct (dev) == FALSE)
    {
      ErrorF("Unable to allocate MAGELLAN FocusClassDeviceStruct\n");
      return !Success;
    }

  if (InitValuatorClassDeviceStruct (dev, 6, xf86GetMotionEvents,
				     pInfo->history_size, Absolute) == FALSE)
    {
      ErrorF ("Unable to allocate MAGELLAN ValuatorClassDeviceStruct\n");
      return !Success;
    }
  else
    {
      for (i = 0; i <= 6; i++)
	{
	  InitValuatorAxisStruct(dev, i, MAGELLAN_MIN, MAGELLAN_MAX, MAGELLAN_RES, 0, MAGELLAN_RES);
	}
    }

#ifdef BELL_FEEDBACK_SUPPORT
  /*
    The InitBellFeedbackClassDeviceStruct function is not exported in the
    4.3.0 or 4.3.1 Xmetro loader. We'll leave this out to stay compatible
  */
  /*
    unsure, whether this still is true for the XFree4.2.0 port. 
    Didn't bother to find out.
    Christoph Koulen
  */

  if (InitBellFeedbackClassDeviceStruct (dev, MagellanBellSound,
					 MagellanBellCtrl) == FALSE)
    {
      ErrorF ("Unable to allocate Magellan BellFeedbackClassDeviceStruct\n");
      return !Success;
    }
#endif

  /* 
   * Allocate the motion events buffer.
   */
  xf86MotionHistoryAllocate (pInfo);
  return (Success);
}

static void
ReadInput (LocalDevicePtr local)
{
  int x, y, z;
  int a, b, c;
  int i, buttons;
  MAGELLANPrivatePtr priv = (MAGELLANPrivatePtr) (local->private);

  /* 
   * set blocking to -1 on the first call because we know there is data to
   * read. Xisb automatically clears it after one successful read so that
   * succeeding reads are preceeded by a select with a 0 timeout to prevent
   * read from blocking indefinitely.
   */
  XisbBlockDuration (priv->buffer, -1);
  while (MAGELLANGetPacket (priv) == Success)
    {
      /* 
       * Examine priv->packet and call these functions as appropriate:
       *
       xf86PostMotionEvent
       xf86PostButtonEvent
      */

      switch (priv->packet[0])
	{
	case 'd':	/* motion packet */
	  if (strlen (priv->packet) == 26)
	    {
	      x = 
		MagellanNibble( priv->packet[1] ) * 4096 +
		MagellanNibble( priv->packet[2] ) * 256 +
		MagellanNibble( priv->packet[3] ) * 16 +
		MagellanNibble( priv->packet[4] ) - 32768;
	      y = 
		MagellanNibble( priv->packet[5] ) * 4096 +
		MagellanNibble( priv->packet[6] ) * 256 +
		MagellanNibble( priv->packet[7] ) * 16 +
		MagellanNibble( priv->packet[8] ) - 32768;
	      z = 
		MagellanNibble( priv->packet[9]  ) * 4096 +
		MagellanNibble( priv->packet[10] ) * 256 +
		MagellanNibble( priv->packet[11] ) * 16 +
		MagellanNibble( priv->packet[12] ) - 32768;
	      
	      a =
		MagellanNibble( priv->packet[13] ) * 4096 +
		MagellanNibble( priv->packet[14] ) * 256 +
		MagellanNibble( priv->packet[15] ) * 16 +
		MagellanNibble( priv->packet[16] ) - 32768;
	      b = 
		MagellanNibble( priv->packet[17] ) * 4096 +
		MagellanNibble( priv->packet[18] ) * 256 +
		MagellanNibble( priv->packet[19] ) * 16 +
		MagellanNibble( priv->packet[20] ) - 32768;
	      c = 
		MagellanNibble( priv->packet[21] ) * 4096 +
		MagellanNibble( priv->packet[22] ) * 256 +
		MagellanNibble( priv->packet[23] ) * 16 +
		MagellanNibble( priv->packet[24] ) - 32768;

	      xf86ErrorFVerb( 5, "Magellan motion %d %d %d -- %d %d %d\n",
			      x, y, z, a, b, c );
	      xf86PostMotionEvent(local->dev, TRUE, 0, 6,
				  x, y, z, a, b, c);
	    }
	  else
	    ErrorF ("Magellan received a short \'d\'packet\n");
	  break;

	case 'k': /* button packet */
	  if (strlen (priv->packet) == 5)
	    {
	      buttons = MagellanNibble( priv->packet[1] ) * 1 +
		        MagellanNibble( priv->packet[2] ) * 16 +
		        MagellanNibble( priv->packet[3] ) * 256; 
	      if (priv->old_buttons != buttons)
		for (i = 0; i < 9; i++)
		  {	
		    if ((priv->old_buttons&(1<<i)) != (buttons&(1<<i)))
		      {
			xf86PostButtonEvent(local->dev, FALSE, i+1,
					    (buttons&(1<<i)), 0, 0);
			xf86ErrorFVerb( 5, "Magellan setting button %d to %d\n",
					i+1, (buttons&(1<<i)) );
		      }
		  }
	      priv->old_buttons = buttons;
	    }
	  else
	    ErrorF ("Magellan received a short \'k\'packet\n");
	  break;
	}
    }
}

static int
ControlProc (LocalDevicePtr local, xDeviceCtl * control)
{
  return (Success);
}

#ifdef BELL_FEEDBACK_SUPPORT
/*
The bell functions are stubbed out for now because they can't be used with the
4.3.0 and 4.3.1 Xmetro binaries. The device can only control the duration of
the beep.
*/
static void
MagellanBellCtrl(DeviceIntPtr dev, BellCtrl *ctrl)
{
}

static void
MagellanBellSound(int percent, DeviceIntPtr dev, pointer ctrl, int unknown)
{
}
#endif

static void
CloseProc (LocalDevicePtr local)
{
}

static int
SwitchMode (ClientPtr client, DeviceIntPtr dev, int mode)
{
  return (Success);
}

/* 
 * The ConvertProc function may need to be tailored for your device.
 * This function converts the device's valuator outputs to x and y coordinates
 * to simulate mouse events.
 */
static Bool
ConvertProc (LocalDevicePtr local,
	     int first,
	     int num,
	     int v0,
	     int v1,
	     int v2,
	     int v3,
	     int v4,
	     int v5,
	     int *x,
	     int *y)
{

  *x = v3;
  *y = v4;
  return (Success);
}

#define WriteString(str)\
XisbWrite (priv->buffer, (unsigned char *)(str), strlen(str)); \
	XisbBlockDuration (priv->buffer, 1000000); \
	if ((MAGELLANGetPacket (priv) != Success) || \
		(strcmp (priv->packet, (str)) != 0)) \
			return (!Success);


static Bool
QueryHardware (MAGELLANPrivatePtr priv)
{
  /* the device resets when the port is opened. Give it time to finish */
  milisleep (1000);

  XisbWrite (priv->buffer, (unsigned char *)MagellanModeOff, strlen(MagellanModeOff));
  XisbBlockDuration (priv->buffer, 200000);
  MAGELLANGetPacket(priv);

  XisbWrite (priv->buffer, (unsigned char *)MagellanInitString, strlen(MagellanInitString));
  XisbBlockDuration (priv->buffer, 200000);
  MAGELLANGetPacket(priv);
  XisbWrite (priv->buffer, (unsigned char *)MagellanInitString, strlen(MagellanInitString));
  XisbBlockDuration (priv->buffer, 200000);
  MAGELLANGetPacket(priv);

  WriteString (MagellanSensitivity);
  WriteString (MagellanPeriod);
  WriteString (MagellanNullRadius);
  WriteString (MagellanMode);
  XisbWrite (priv->buffer, (unsigned char *)MagellanVersion, strlen(MagellanVersion));


  /* block for up to 1 second while trying to read the response */
  XisbBlockDuration (priv->buffer, 1000000);
  NewPacket (priv);
	
  if ((MAGELLANGetPacket (priv) == Success) && (priv->packet[0] == 'v'))
    {
      priv->packet[strlen(priv->packet) - 1] = '\0';
      xf86MsgVerb( X_PROBED, 3, " initialized: %s\n",  &(priv->packet[3]) );
    }
  else
    return (!Success);

  return (Success);
}

static void
NewPacket (MAGELLANPrivatePtr priv)
{
  priv->lex_mode = magellan_normal;
  priv->packeti = 0;
}

static Bool
MAGELLANGetPacket (MAGELLANPrivatePtr priv)
{
  int count = 0;
  int c;

  while ((c = XisbRead (priv->buffer)) >= 0)
    {
      /* 
       * fail after 500 bytes so the server doesn't hang forever if a
       * device sends bad data.
       */
      if (count++ > 500)
	{
	  NewPacket (priv);
	  return (!Success);
	}

      switch (priv->lex_mode)
	{
	case magellan_normal:
	  if (priv->packeti > MAGELLAN_PACKET_SIZE)
	    {
	      NewPacket (priv);
	      return (!Success);
	    }
	  priv->packet[priv->packeti] = c;
	  priv->packeti++;

	  /* BEWARE: SpaceMouse echoes back most everything you send her.
	   * be sure NOT to send command strings that contain "\r" somewhere in the middle!
	   * "Success" of this function  will be reported upon the first "\r" received".
	   * You might end up with the rest of a SpaceMouse echo still waiting in the
	   * buffer, which would make the comparison between command sent and the answer
	   * returned by SpaceMouse fail for the next command!
	   */
	  if (c == '\r')
	    {
	      priv->packet[priv->packeti] = '\0';
	      NewPacket (priv);
	      return (Success);
	    }
	  break;
	}
    }
  return (!Success);
}















