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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/spaceorb/spaceorb.c,v 1.13 2001/11/26 16:25:54 dawes Exp $ */

#define _SPACEORB_C_
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
#include "spaceorb.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/


/*****************************************************************************
 *	Local Variables
 ****************************************************************************/
/** @added by Guido Heumer */
InputDriverRec SPACEORB = {
        1,
        "spaceorb",
        NULL,
        SpaceorbPreInit,
        /*DynaproUnInit*/NULL,
        NULL,
        0
};    

#ifdef XFree86LOADER

static XF86ModuleVersionInfo VersionRec =
{
	"spaceorb",
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
SPACEORBSetupProc(pointer module,
		  pointer options,
		  int *errmaj,
		  int *errmin ) {
  xf86LoaderReqSymLists(reqSymbols, NULL);
  xf86AddInputDriver(&SPACEORB, module, 0);
  return (pointer) 1;
}

static void
TearDownProc( pointer p )
{
  if (!xf86ServerIsOnlyDetecting()) {

    InputInfoPtr pInfo = (InputInfoPtr) p;
    SPACEORBPrivatePtr priv = (SPACEORBPrivatePtr) pInfo->private;

    DeviceOff (pInfo->dev);
  
    xf86CloseSerial (pInfo->fd);
    XisbFree (priv->buffer);
    xfree (priv);
    xfree (pInfo->name);
    xfree (pInfo);
  }
}

/* was before: XF86ModuleData spaceorbModuleData = { &VersionRec, SetupProc, TearDownProc }; */
XF86ModuleData spaceorbModuleData = { &VersionRec, SPACEORBSetupProc, TearDownProc };

#endif /* XFree86LOADER */


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
	"Vmin", "5",
	"Vtime", "1",
	"FlowControl", "None",
	NULL,
};

static char spaceware[] = "0SpaceWare!";

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static InputInfoPtr
SpaceorbPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{              
        InputInfoPtr pInfo;
        SPACEORBPrivatePtr priv = xcalloc (1, sizeof (SPACEORBPrivateRec));

	if (!priv)
		return NULL;

        if (!(pInfo = xf86AllocateInput(drv, 0))) {
		xfree(priv);
                return NULL;
	}
  
	priv->lex_mode = SPACEORB_normal;
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
	  ErrorF ("SPACEORB driver unable to open device\n");
	  goto SetupProc_fail;
        }
	
	priv->buffer = XisbNew (pInfo->fd, 200);
	DBG (9, XisbTrace (priv->buffer, 1));

	/* 
	 * Verify the hardware is attached and functional
	 */
        if (QueryHardware (priv) != Success)
        {
	  ErrorF ("Unable to query/initialize SpaceOrb hardware.\n");
	  goto SetupProc_fail;
        }

        /* this results in an xstrdup that must be freed later */
        pInfo->name = xf86SetStrOption( pInfo->options, "DeviceName", "SpaceOrb XInput Device");
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

  unsigned char map[] = {0, 1, 2, 3, 4, 5, 6, 7};
  
  if (InitButtonClassDeviceStruct (dev, 7, map) == FALSE) {
    ErrorF ("Unable to allocate SPACEORB ButtonClassDeviceStruct\n");
    return !Success;
  }

  if (InitFocusClassDeviceStruct (dev) == FALSE) {
    ErrorF("Unable to allocate SPACEORB FocusClassDeviceStruct\n");
    return !Success;
  }

  if (InitValuatorClassDeviceStruct (dev, 6, xf86GetMotionEvents,
				     pInfo->history_size, Absolute) == FALSE)
    {
      ErrorF ("Unable to allocate SPACEORB ValuatorClassDeviceStruct\n");
      return !Success;
    }
  else
    {
      InitValuatorAxisStruct(dev, 0, -512, 511, 9600,0,9600);
      InitValuatorAxisStruct(dev, 1, -512, 511, 9600,0,9600);
      InitValuatorAxisStruct(dev, 2, -512, 511, 9600,0,9600);
      InitValuatorAxisStruct(dev, 3, -512, 511, 9600,0,9600);
      InitValuatorAxisStruct(dev, 4, -512, 511, 9600,0,9600);
      InitValuatorAxisStruct(dev, 5, -512, 511, 9600,0,9600);
    }

  /* 
   * Allocate the motion events buffer.
   */
  xf86MotionHistoryAllocate (pInfo);
  return (Success);
}

/* 
 * The ReadInput function will have to be tailored to your device
 */
static void
ReadInput (LocalDevicePtr local)
{
  int x, y, z, u, v, r;
  int buttons;
  int i;
  SPACEORBPrivatePtr priv = (SPACEORBPrivatePtr) (local->private);
  
  /* 
   * set blocking to -1 on the first call because we know there is data to
   * read. Xisb automatically clears it after one successful read so that
   * succeeding reads are preceeded buy a select with a 0 timeout to prevent
   * read from blocking indefinately.
   */
  XisbBlockDuration (priv->buffer, -1);
  while (SPACEORBGetPacket (priv) == Success)
    {
      /* 
       * Examine priv->packet and call these functions as appropriate:
       *
       xf86PostMotionEvent
       xf86PostButtonEvent
      */
      switch (priv->packet_type)
	{
	case SPACEORB_D_PACKET:
	  /* Turn chars into 10 bit integers */
	  x = ((priv->packet[1] & 0177)<<3)|((priv->packet[2] & 0160)>>4);
	  y = ((priv->packet[2] & 0017)<<6)|((priv->packet[3] & 0176)>>1);
	  z = ((priv->packet[3] & 0001)<<9)|((priv->packet[4] & 0177)<<2)|((priv->packet[5] & 0140)>>5);
	  u = ((priv->packet[5] & 0037)<<5)|((priv->packet[6] & 0174)>>2);
	  v = ((priv->packet[6] & 0003)<<8)|((priv->packet[7] & 0177)<<1)|((priv->packet[8] & 0100)>>6);
	  r = ((priv->packet[8] & 0077)<<4)|((priv->packet[9] & 0170)>>3);
	  
	  /* Get the sign right. */
	  if (x > 511) x -= 1024;
	  if (y > 511) y -= 1024;
	  if (z > 511) z -= 1024;
	  if (u > 511) u -= 1024;
	  if (v > 511) v -= 1024;
	  if (r > 511) r -= 1024;
	  
	  xf86ErrorFVerb( 9, "SpaceOrb motion %d %d %d -- %d %d %d\n",
			  x, y, z, u, v, r );
	  xf86PostMotionEvent(local->dev, TRUE, 0, 6, x, y, z, u, v, r);
	  break;
	  
	case SPACEORB_K_PACKET:
	  buttons = priv->packet[1];
	  if (priv->old_buttons != buttons)
	    for (i = 0; i < 7; i++)
	      {	
		if ((priv->old_buttons&(1<<i)) != (buttons&(1<<i)))
		  {
		    xf86PostButtonEvent(local->dev, FALSE, i+1,
					(buttons&(1<<i)), 0, 0);
		    xf86ErrorFVerb( 9, "SpaceOrb setting button %d to %d\n",
				    i+1, (buttons&(1<<i)) );
		  }
	      }
	  priv->old_buttons = buttons;
	  break;
	}
    }
}

/* 
 * The ControlProc function may need to be tailored for your device
 */
static int
ControlProc (LocalDevicePtr local, xDeviceCtl * control)
{
	return (Success);
}

/* 
 * the CloseProc should not need to be tailored to your device
 */
static void
CloseProc (LocalDevicePtr local)
{
}

/* 
 * The SwitchMode function may need to be tailored for your device
 */
static int
SwitchMode (ClientPtr client, DeviceIntPtr dev, int mode)
{
	return (Success);
}

/*
 * this probably doesn't make much sense for a spaceorb
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

/* 
 * the QueryHardware fuction should be tailored to your device to
 * verify the device is attached and functional and perform any
 * needed initialization.
 */
static Bool
QueryHardware (SPACEORBPrivatePtr priv)
{
  /*
   * When you open the serial port, the spaceorb sends a greeting packet
   * make sure we got it to verify that the device is present.
   * block for up to a second to make sure the data is there.
   */
  XisbBlockDuration (priv->buffer, 1000000);
  if ((SPACEORBGetPacket (priv) == Success) &&
      (priv->packet_type == SPACEORB_R_PACKET))
    return (Success);
  else
    {
      ErrorF ("No response from SpaceOrb hardware.\n");
      return (!Success);
    }
}

static void
NewPacket (SPACEORBPrivatePtr priv)
{
  priv->lex_mode = SPACEORB_normal;
  priv->packeti = 0;
}

static Bool
SPACEORBGetPacket (SPACEORBPrivatePtr priv)
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
	  if (c == '\r')
	    continue;
	  
	  switch (priv->lex_mode)
	    {
	    case SPACEORB_normal:
	      if (c == SPACEORB_R_PACKET)
		{
		  xf86ErrorFVerb( 9, "SpaceOrb got an R packet\n" );
		  priv->packet_type = SPACEORB_R_PACKET;
		  priv->lex_mode = SPACEORB_body;
		  priv->expected_len = SPACEORB_R_BODY_LEN;
		}
	      else if (c == SPACEORB_K_PACKET)
		{
		  priv->packet_type = SPACEORB_K_PACKET;
		  priv->lex_mode = SPACEORB_body;
		  priv->expected_len = SPACEORB_K_BODY_LEN;
		}
	      else if (c == SPACEORB_D_PACKET)
		{
		  xf86ErrorFVerb( 9, "SpaceOrb got a D packet\n" );
		  priv->packet_type = SPACEORB_D_PACKET;
		  priv->lex_mode = SPACEORB_Dbody;
		  priv->expected_len = SPACEORB_D_BODY_LEN;
		}
	      break;
	    case SPACEORB_Dbody:
	      if (priv->packeti < priv->expected_len)
		{
		  xf86ErrorFVerb( 9, "\t%d = %c data = %c %d\n",
				  priv->packeti, spaceware[priv->packeti],
				  (c & 0x7F), (c & 0x7F) );
		  priv->packet[priv->packeti] =
		    (unsigned char) (c & 0x7F) ^ spaceware[priv->packeti];
		  priv->packeti++;
		}
	      if (priv->packeti == priv->expected_len)
		{
#ifdef DEBUG
ErrorF ("returning packet <");
 {
	int i;
	for (i = 0; i < priv->expected_len; i++)
		ErrorF ("%c", priv->packet[i]);
ErrorF (">\n");
}
#endif
                      NewPacket (priv);
				return (Success);
			}
			break;

	    case SPACEORB_body:
	      if (priv->packeti < priv->expected_len)
		priv->packet[priv->packeti++] = (unsigned char) c & 0x7F;
	      if (priv->packeti == priv->expected_len)
		{
		  NewPacket (priv);
		  return (Success);
		}
	      break;
	      
	    }
	}
	return (!Success);
}









