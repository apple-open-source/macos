/* 
 * Copyright (c) 1999  Machine Vision Holdings Incorporated
 * Author: Mayk Langer <langer@vsys.de>
 *
 * Template driver used: Copyright (c) 1998  Metro Link Incorporated
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
  * Changes :
  * 25-Jul-2003 Andreas Kies ( a.kies(at)gppag.de )
  *   - Make driver work again ; Pen-up is one byte in size
  *   - Change extremly missleading error messages
  *   - Make dectection work after restart of X
  */

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/dmc/xf86DMC.c,v 1.3 2003/06/25 18:06:25 eich Exp $ */

#define _DMC_C_

#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#include <xf86_ansic.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <xisb.h>
#include <exevents.h>

#include "xf86DMC.h"

InputDriverRec DMC = {
        1,
        "dmc",
        NULL,
        DMCPreInit,
        /*DMCUnInit*/NULL,
        NULL,
        0
};        



#ifdef XFree86LOADER

static XF86ModuleVersionInfo VersionRec =
{
	"dmc",
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
DMCSetupProc(	pointer module,
			pointer options,
			int *errmaj,
			int *errmin )
{
	xf86LoaderReqSymLists(reqSymbols, NULL);
	xf86AddInputDriver(&DMC, module, 0);
	return (pointer) 1;
}

XF86ModuleData dmcModuleData = { &VersionRec, DMCSetupProc, NULL };


#endif /* XFree86LOADER */


/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning
 */
static const char *default_options[] =
{
	/*	"Device", "/dev/ttyS1",*/
	"BaudRate", "9600",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "None",
	"Vmin", "3",
	"Vtime", "1",
	"FlowControl", "None",
	NULL,
};


/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/



static InputInfoPtr
DMCPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{              
	InputInfoPtr pInfo;
   	DMCPrivatePtr priv = xcalloc (1, sizeof (DMCPrivateRec));
	char *s;

	if (!priv)
		return NULL;

	if (!(pInfo = xf86AllocateInput(drv, 0))) {
		xfree(priv);
		return NULL;
	}
  
	priv->min_x = 0;
	priv->max_x = 1024;
	priv->min_y = 768;
	priv->max_y = 0;
	priv->screen_num = 0;
	priv->screen_width = -1;
	priv->screen_height = -1;
	priv->lex_mode = DMC_byte0;
	priv->swap_xy = 0;
	priv->button_down = FALSE;
	priv->button_number = 1;
	priv->proximity = FALSE;
	priv->pen_down = 0;

	pInfo->type_name = XI_TOUCHSCREEN;
	pInfo->device_control = DeviceControl;
	pInfo->read_input = ReadInput;
	pInfo->control_proc = ControlProc;
	pInfo->close_proc = CloseProc;
	pInfo->switch_mode = SwitchMode;
	pInfo->conversion_proc = ConvertProc;
	pInfo->dev = NULL;
	pInfo->private = priv;
	pInfo->private_flags = 0;
	pInfo->flags = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
	pInfo->conf_idev = dev;

	xf86CollectInputOptions(pInfo, default_options, NULL);

	xf86OptionListReport( pInfo->options );

	pInfo->fd = xf86OpenSerial (pInfo->options);
	if (pInfo->fd == -1)
	{
		ErrorF ("DMC driver unable to open device\n");
		goto SetupProc_fail;
	}
	xf86CloseSerial(pInfo->fd);
	/* 
	 * Process the options for your device like this
	 */
	priv->min_x = xf86SetIntOption( pInfo->options, "MinX", 0 );
	priv->max_x = xf86SetIntOption( pInfo->options, "MaxX", 1024 );
	priv->min_y = xf86SetIntOption( pInfo->options, "MinY", 768 );
	priv->max_y = xf86SetIntOption( pInfo->options, "MaxY", 0 );
	priv->screen_num = xf86SetIntOption( pInfo->options, "ScreenNumber", 0 );
	priv->button_number = xf86SetIntOption( pInfo->options, "ButtonNumber", 1 );
	priv->swap_xy = xf86SetIntOption( pInfo->options, "SwapXY", 0 );
	priv->buffer = NULL;
	s = xf86FindOptionValue (pInfo->options, "ReportingMode");
	if ((s) && (xf86NameCmp (s, "raw") == 0))
		priv->reporting_mode = TS_Raw;
	else
		priv->reporting_mode = TS_Scaled;

	priv->proximity = FALSE;
	priv->button_down = FALSE;
	priv->lex_mode = DMC_byte0;

	if (QueryHardware (priv) != Success)
	{
		ErrorF ("Unable to query/initialize DMC hardware.\n");
		goto SetupProc_fail;
	}

	/* this results in an xstrdup that must be freed later */
	pInfo->name = xf86SetStrOption( pInfo->options, "DeviceName", "DMC");
	xf86ProcessCommonOptions(pInfo, pInfo->options);

	pInfo->flags |= XI86_CONFIGURED;
	return (pInfo);

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
	InputInfoPtr pInfo = dev->public.devicePrivate;
	DMCPrivatePtr priv = (DMCPrivatePtr) (pInfo->private);
	unsigned char map[] =
	{0, 1};

	switch (mode)
	{
	case DEVICE_INIT:
		/*
		 * these have to be here instead of in the SetupProc, because when the
		 * SetupProc is run at server startup, screenInfo is not setup yet
		 */
		priv->screen_width = screenInfo.screens[priv->screen_num]->width;
		priv->screen_height = screenInfo.screens[priv->screen_num]->height;
		
		/*
		 * Device reports button press for 1 button.
		 */
		if (InitButtonClassDeviceStruct (dev, 1, map) == FALSE)
			{
				ErrorF ("Unable to allocate DMC ButtonClassDeviceStruct\n");
				return !Success;
			}
		
		/*
		 * Device reports motions on 2 axes in absolute coordinates.
		 * Axes min and max values are reported in raw coordinates.
		 */
		if (InitValuatorClassDeviceStruct (dev, 2, xf86GetMotionEvents,
						   pInfo->history_size, Absolute) == FALSE)
			{
				ErrorF ("Unable to allocate DMC ValuatorClassDeviceStruct\n");
				return !Success;
			}
		else
			{
				InitValuatorAxisStruct (dev, 0, priv->min_x, priv->max_x,
							9500,
							0 /* min_res */ ,
							9500 /* max_res */ );
				InitValuatorAxisStruct (dev, 1, priv->min_y, priv->max_y,
							10500,
							0 /* min_res */ ,
							10500 /* max_res */ );
			}
		
		if (InitProximityClassDeviceStruct (dev) == FALSE)
			{
				ErrorF ("unable to allocate DMC ProximityClassDeviceStruct\n");
				return !Success;
			}
		
		if (InitPtrFeedbackClassDeviceStruct(dev, DMCPtrCtrl) == FALSE)
			{
				ErrorF ("unable to allocate DMC PtrFeedbackClassDeviceStruct\n");
				return !Success;
			}
		
		/* 
		 * Allocate the motion events buffer.
		 */
		xf86MotionHistoryAllocate (pInfo);
		return (Success);
		
	case DEVICE_ON:
		pInfo->fd = xf86OpenSerial(pInfo->options);
		if (pInfo->fd == -1)
		{
			xf86Msg(X_WARNING, "%s: cannot open input device\n", pInfo->name);
			return (!Success);
		}
		else 
		{
			priv->buffer = XisbNew(pInfo->fd, 64);
			if (!priv->buffer) 
			{
				xf86CloseSerial(pInfo->fd);
				pInfo->fd = -1;
				return (!Success);
			}
			else
			{
				unsigned char	resbuf[1] = { 0x55 };
				unsigned char	buf[2] = { 0x05, 0x40 };

				XisbBlockDuration (priv->buffer, 500000);
				sleep(1);	/* touch need ca. 500ms delay !!! */
				if ( DMCSendPacket(priv, resbuf, 1) != Success )
				{
					xf86Msg(X_ERROR, "DMC-Touch reset error\n");
					return (!Success);
				}

				sleep(1);	/* touch need ca. 500ms delay !!! */
				if ( DMCSendPacket(priv, buf, 2) == Success )
				{
					/* wait for right response */
					priv->lex_mode = DMC_Response0;
					if (DMCGetPacket (priv) == Success )
					{
						if ( priv->packet[0] == 0x06 )
						{
							buf[0] = 0x31;
							DMCSendPacket(priv,buf,1);
							priv->lex_mode = DMC_Response0;
							xf86Msg(X_INFO, "DMC-Touch found\n");
						}
						else
						{
							xf86Msg(X_ERROR, "DMC-Touch not found(bad response)\n");
							return (!Success);
						}
					}
					else
					{
						xf86Msg(X_ERROR, "DMC-Touch not found(no response)\n");
						return (!Success);
					}
				}
				else
				{
					xf86Msg(X_ERROR, "DMC-Touch not found(send error)\n");
					return (!Success);
				}
			}
		}

		XisbBlockDuration (priv->buffer, -1);
		priv->lex_mode = DMC_byte0;
		
		xf86FlushInput(pInfo->fd);
		AddEnabledDevice (pInfo->fd);
		dev->public.on = TRUE;
		return (Success);
		
	case DEVICE_OFF:
	case DEVICE_CLOSE:
		if (pInfo->fd != -1)
			{ 
				RemoveEnabledDevice (pInfo->fd);
				if (priv->buffer)
					{
						XisbFree(priv->buffer);
						priv->buffer = NULL;
					}
				xf86CloseSerial(pInfo->fd);
			}
		dev->public.on = FALSE;
		return (Success);
	default:
		return (BadValue);
	}

}


/* 
 * The ReadInput function will have to be tailored to your device
 */
static void
ReadInput (InputInfoPtr pInfo)
{
	DMCPrivatePtr priv = (DMCPrivatePtr) (pInfo->private);
	int x,y;
	unsigned char opck[ DMC_PACKET_SIZE ];

	/* 
	 * set blocking to -1 on the first call because we know there is data to
	 * read. Xisb automatically clears it after one successful read so that
	 * succeeding reads are preceeded buy a select with a 0 timeout to prevent
	 * read from blocking indefinately.
	 */
	XisbBlockDuration (priv->buffer, -1);
	while (1)
	{
		memcpy(opck,priv->packet,5);

		if ( DMCGetPacket (priv) != Success)
			break;
		if ( priv->packet[0] == 0x11 )
			priv->pen_down=1;
		else
			priv->pen_down=0;

		if ( priv->swap_xy)
		{
			y = priv->packet[1]*256+priv->packet[2];
			x = priv->packet[3]*256+priv->packet[4];
		}
		else
		{
			x = priv->packet[1]*256+priv->packet[2];
			y = priv->packet[3]*256+priv->packet[4];
		}
		priv->packet[0] = priv->pen_down ? 0x01 : 0x00;

		if (priv->reporting_mode == TS_Scaled)
		{	
			x = xf86ScaleAxis (x, 0, priv->screen_width, priv->min_x,
					   priv->max_x);
                        y = xf86ScaleAxis (y, 0, priv->screen_height, priv->min_y,
					   priv->max_y);
                }        		

		xf86XInputSetScreen (pInfo, priv->screen_num, x, y);

		if ((priv->proximity == FALSE) && (priv->packet[0] & 0x01))
		{
			priv->proximity = TRUE;
			xf86PostProximityEvent (pInfo->dev, 1, 0, 2, x, y);
		}              	
		
             /*
                 * Send events.
                 *
                 * We *must* generate a motion before a button change if pointer
                 * location has changed as DIX assumes this. This is why we always
                 * emit a motion, regardless of the kind of packet processed.
                 */

                xf86PostMotionEvent (pInfo->dev, TRUE, 0, 2, x, y);

                /*
                 * Emit a button press or release.
                 */
                if ((priv->button_down == FALSE) && (priv->packet[0] & 0x01))

                {
                        xf86PostButtonEvent (pInfo->dev, TRUE,
					     priv->button_number, 1, 0, 2, x, y);
                        priv->button_down = TRUE;
                }
                if ((priv->button_down == TRUE) && !(priv->packet[0] & 0x01))
                {
                        xf86PostButtonEvent (pInfo->dev, TRUE,
					     priv->button_number, 0, 0, 2, x, y);
                        priv->button_down = FALSE;
                }
                /*
                 * the untouch should always come after the button release
                 */
                if ((priv->proximity == TRUE) && !(priv->packet[0] & 0x01))
                {
                        priv->proximity = FALSE;
                        xf86PostProximityEvent (pInfo->dev, 0, 0, 2, x, y);
                }
	}
}

/* 
 * The ControlProc function may need to be tailored for your device
 */
static int
ControlProc (InputInfoPtr pInfo, xDeviceCtl * control)
{
	xDeviceTSCalibrationCtl *c = (xDeviceTSCalibrationCtl *) control;
	DMCPrivatePtr priv = (DMCPrivatePtr) (pInfo->private);

        priv->min_x = c->min_x;
        priv->max_x = c->max_x;
        priv->min_y = c->min_y;
        priv->max_y = c->max_y;
 
	return (Success);
}

/* 
 * the CloseProc should not need to be tailored to your device
 */
static void
CloseProc (InputInfoPtr pInfo)
{

}

/* 
 * The SwitchMode function may need to be tailored for your device
 */
static int
SwitchMode (ClientPtr client, DeviceIntPtr dev, int mode)
{
	InputInfoPtr pInfo = dev->public.devicePrivate;
        DMCPrivatePtr priv = (DMCPrivatePtr) (pInfo->private);


	if ((mode == TS_Raw) || (mode == TS_Scaled))
        {
                priv->reporting_mode = mode;
                return (Success);
        }
        else if ((mode == SendCoreEvents) || (mode == DontSendCoreEvents))
        {
                xf86XInputSetSendCoreEvents (pInfo, (mode == SendCoreEvents));
                return (Success);
        }
        else
                return (!Success);   
}

/* 
 * The ConvertProc function may need to be tailored for your device.
 * This function converts the device's valuator outputs to x and y coordinates
 * to simulate mouse events.
 */
static Bool
ConvertProc (InputInfoPtr pInfo,
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
	DMCPrivatePtr priv = (DMCPrivatePtr) (pInfo->private);

	if (priv->reporting_mode == TS_Raw)
	{
                *x = xf86ScaleAxis (v0, 0, priv->screen_width, priv->min_x,
                                                        priv->max_x);
                *y = xf86ScaleAxis (v1, 0, priv->screen_height, priv->min_y,
                                                        priv->max_y);
        }
        else
        {
                *x = v0;
                *y = v1;
	}
	return (TRUE);
}

/* 
 * the QueryHardware fuction should be tailored to your device to
 * verify the device is attached and functional and perform any
 * needed initialization.
 */
static Bool
QueryHardware (DMCPrivatePtr priv)
{
	/* Maybe once we get the hardware to actually respond correctly to its 
	   configuration 'packets' */

	return (Success);
}

/* 
 * This function should be renamed for your device and tailored to handle
 * your device's protocol.
 */
static Bool
DMCGetPacket (DMCPrivatePtr priv)
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
			return (!Success);

		switch (priv->lex_mode)
		{
		case DMC_byte0:
			if ((( c & 0x3f ) != 0x11 ) &&
				(( c & 0x3f ) != 0x10 ))
					return (!Success);

			priv->packet[0] = (unsigned char) c & 0x3f;
			if ( ( (unsigned char) c & 0x3f) == 0x10 )
				return (Success);
			priv->lex_mode = DMC_byte1;
			break;

		case DMC_byte1:
			priv->packet[1] = (unsigned char) c;
			priv->lex_mode = DMC_byte2;
			break;
			
		case DMC_byte2:
			priv->packet[2] = (unsigned char) c;
			priv->lex_mode = DMC_byte3;
			break;

		case DMC_byte3:
			priv->packet[3] = (unsigned char) c;
			priv->lex_mode = DMC_byte4;
			break;

		case DMC_byte4:
			priv->packet[4] = (unsigned char) c;
			priv->lex_mode = DMC_byte0;
			return (Success);
			break;

		case DMC_Response0:
			priv->packet[0] = (unsigned char) c;
			return (Success);
			break;

		}
	}
	return (!Success);
}

static Bool
DMCSendPacket (DMCPrivatePtr priv, unsigned char *buf, int len)
{
	int				count = 0;

	while ( len > 0 )
	{
		if ( XisbWrite(priv->buffer, buf, 1) == 1 )
		{
			buf++;
			len--;
			continue;
		}
		if ( count++ > 500 )
			break;
	}
	return (len ? !Success : Success);
}

static void
DMCPtrCtrl(DeviceIntPtr device, PtrCtrl *ctrl)
{
  /* I have no clue what this does, except that registering it stops the 
     X server segfaulting in ProcGetPointerMapping()
     Ho Hum.
  */
}
