/* 
 * Copyright (c) 1999  Machine Vision Holdings Incorporated
 * Author: David Woodhouse <David.Woodhouse@mvhi.com>
 * CoAuthor: Mayk Langer <langer@vsys.de>
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/penmount/xf86PM.c,v 1.3 2001/11/26 16:25:53 dawes Exp $ */

#define _PENMOUNT_C_

#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#include <xf86_ansic.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <xisb.h>
#include <exevents.h>

#include "xf86PM.h"

InputDriverRec PENMOUNT = {
        1,
        "penmount",
        NULL,
        PenMountPreInit,
        /*PenMountUnInit*/NULL,
        NULL,
        0
};        



#ifdef XFree86LOADER

static XF86ModuleVersionInfo VersionRec =
{
	"penmount",
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
PenMountSetupProc(	pointer module,
			pointer options,
			int *errmaj,
			int *errmin )
{
	xf86LoaderReqSymLists(reqSymbols, NULL);
	xf86AddInputDriver(&PENMOUNT, module, 0);
	return (pointer) 1;
}

XF86ModuleData penmountModuleData = { &VersionRec, PenMountSetupProc, NULL };


#endif /* XFree86LOADER */


/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning
 */
static const char *default_options[] =
{
	/*	"Device", "/dev/ttyS1",*/
	"BaudRate", "19200",
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
PenMountPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{              
	InputInfoPtr pInfo;
   	PenMountPrivatePtr priv = xcalloc (1, sizeof (PenMountPrivateRec));
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
	priv->lex_mode = PenMount_byte0;
	priv->swap_xy = 0;
	priv->button_down = FALSE;
	priv->button_number = 1;
	priv->proximity = FALSE;
	priv->chip = CHIP_UNKNOWN;
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
		ErrorF ("PenMount driver unable to open device\n");
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
	priv->lex_mode = PenMount_byte0;

	if (QueryHardware (priv) != Success)
	{
		ErrorF ("Unable to query/initialize PenMount hardware.\n");
		goto SetupProc_fail;
	}

	/* this results in an xstrdup that must be freed later */
	pInfo->name = xf86SetStrOption( pInfo->options, "DeviceName", "PenMount");
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
	PenMountPrivatePtr priv = (PenMountPrivatePtr) (pInfo->private);
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
				ErrorF ("Unable to allocate PenMount ButtonClassDeviceStruct\n");
				return !Success;
			}
		
		/*
		 * Device reports motions on 2 axes in absolute coordinates.
		 * Axes min and max values are reported in raw coordinates.
		 */
		if (InitValuatorClassDeviceStruct (dev, 2, xf86GetMotionEvents,
						   pInfo->history_size, Absolute) == FALSE)
			{
				ErrorF ("Unable to allocate PenMount ValuatorClassDeviceStruct\n");
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
				ErrorF ("unable to allocate PenMount ProximityClassDeviceStruct\n");
				return !Success;
			}
		
		if (InitPtrFeedbackClassDeviceStruct(dev, PenMountPtrCtrl) == FALSE)
			{
				ErrorF ("unable to allocate PenMount PtrFeedbackClassDeviceStruct\n");
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
/*			if (isatty (pInfo->fd))		check if DMC8910 is found */
			{
/* echo Success Code */
				unsigned char	buf[5] = { 'D', 'G', 0x02, 0x80, 0x00 };

				XisbBlockDuration (priv->buffer, 500000);
				if ( PenMountSendPacket(priv, buf, 5) == Success )
				{
					/* wait for right response */
					priv->lex_mode = PenMount_Response0;
					if (PenMountGetPacket (priv) == Success )
					{
						if (( priv->packet[0] == 0xff ) &&
							( priv->packet[1] == 0x70 ) )
						{
							priv->chip = DMC8910;
						/* disable DMC8910 */
							buf[2] = 0x0b;
							buf[3] = 0x00;
							buf[4] = 0x00;
							PenMountSendPacket(priv,buf,5);
							priv->lex_mode = PenMount_Response0;
							PenMountGetPacket(priv);
						/* set screen width */
							buf[2] = 0x02;
							buf[3] = (priv->screen_width & 0x0fff) >> 8;
							buf[4] = priv->screen_width & 0xff;
							PenMountSendPacket(priv,buf,5);
							priv->lex_mode = PenMount_Response0;
							PenMountGetPacket(priv);
						/* set screen height */
							buf[2] = 0x02;
							buf[3] = (priv->screen_height & 0x0fff) >> 8;
							buf[4] = priv->screen_height & 0xff;
							buf[3] |= 0x10;
							PenMountSendPacket(priv,buf,5);
							priv->lex_mode = PenMount_Response0;
							PenMountGetPacket(priv);
						/* enable DMC8910 */
							buf[2] = 0x0a;
							buf[3] = 0x01;
							buf[4] = 0x00;
							PenMountSendPacket(priv,buf,5);
							priv->lex_mode = PenMount_Response0;
							PenMountGetPacket(priv);
						}
					}
				}
			}
		}

		XisbBlockDuration (priv->buffer, -1);
		priv->lex_mode = PenMount_byte0;
		
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
	PenMountPrivatePtr priv = (PenMountPrivatePtr) (pInfo->private);
	int x,y;
	unsigned char opck[ PENMOUNT_PACKET_SIZE ];

	/* 
	 * set blocking to -1 on the first call because we know there is data to
	 * read. Xisb automatically clears it after one successful read so that
	 * succeeding reads are preceeded buy a select with a 0 timeout to prevent
	 * read from blocking indefinately.
	 */
	XisbBlockDuration (priv->buffer, -1);
	while (1)
	{
		if ( priv->chip != DMC8910 )
		{
			if ( PenMountGetPacket (priv) != Success)
				break;
			if (( priv->packet[0] & 0x0C ) != 0x0C )
				break;
			if (priv->swap_xy) {
				y = priv->packet[1];
				y |= (priv->packet[0] & 0x10) ? 0x100 : 0;
				y |= (priv->packet[0] & 0x40) ? 0x200 : 0;
				x = priv->packet[2];
				x |= (priv->packet[0] & 0x20) ? 0x100 : 0;
				x |= (priv->packet[0] & 0x80) ? 0x200 : 0;
			} else {
				x = priv->packet[1];
				x |= (priv->packet[0] & 0x10) ? 0x100 : 0;
				x |= (priv->packet[0] & 0x40) ? 0x200 : 0;
				y = priv->packet[2];
				y |= (priv->packet[0] & 0x20) ? 0x100 : 0;
				y |= (priv->packet[0] & 0x80) ? 0x200 : 0;
			}
		}
		else
		{
			memcpy(opck,priv->packet,5);
			if ( PenMountGetPacket (priv) != Success )
				break;
			if (( priv->packet[1] == 0xfd ) && ( priv->packet[2] == 0xfd ))
			{
				priv->pen_down = 1;
				continue;
			}
			if (( priv->packet[1] == 0xfe ) && ( priv->packet[2] == 0xfe ))
			{
				memcpy(priv->packet,opck,5);
				if ( !priv->pen_down )
					continue;
				priv->pen_down = 0;
			}
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
		}

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
	PenMountPrivatePtr priv = (PenMountPrivatePtr) (pInfo->private);

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
        PenMountPrivatePtr priv = (PenMountPrivatePtr) (pInfo->private);


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
	PenMountPrivatePtr priv = (PenMountPrivatePtr) (pInfo->private);

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
QueryHardware (PenMountPrivatePtr priv)
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
PenMountGetPacket (PenMountPrivatePtr priv)
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
		case PenMount_byte0:
			if ( priv->chip != DMC8910 )
			{
				if (!(c & 0x08) )
					return (!Success);
			}
			else
			{
				if ( c != 0xff )
					return (!Success);
			}
			priv->packet[0] = (unsigned char) c;
			priv->lex_mode = PenMount_byte1;
			break;

		case PenMount_byte1:
			priv->packet[1] = (unsigned char) c;
			priv->lex_mode = PenMount_byte2;
			break;
			
		case PenMount_byte2:
			priv->packet[2] = (unsigned char) c;
			priv->lex_mode = PenMount_byte0;
			if ( priv->chip != DMC8910 )
				return (Success);
			if (( priv->packet[2] == 0xfe ) && ( priv->packet[1] == 0xfe ))
				return (Success);
			if (( priv->packet[2] == 0xfd ) && ( priv->packet[1] == 0xfd ))
				return (Success);
			priv->lex_mode = PenMount_byte3;
			break;

		case PenMount_byte3:
			priv->packet[3] = (unsigned char) c;
			priv->lex_mode = PenMount_byte4;
			break;

		case PenMount_byte4:
			priv->packet[4] = (unsigned char) c;
			priv->lex_mode = PenMount_byte0;
			return (Success);
			break;

		case PenMount_Response0:
			if ( c == 0xff )
				priv->lex_mode = PenMount_Response1;
			priv->packet[0] = (unsigned char) c;
			break;

		case PenMount_Response1:
			priv->packet[1] = (unsigned char) c;
			priv->lex_mode = PenMount_Response2;
			break;
		case PenMount_Response2:
			priv->packet[2] = (unsigned char) c;
			priv->lex_mode = PenMount_byte0;
			return (Success);
			break;
		}
	}
	return (!Success);
}

static Bool
PenMountSendPacket (PenMountPrivatePtr priv, unsigned char *buf, int len)
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
PenMountPtrCtrl(DeviceIntPtr device, PtrCtrl *ctrl)
{
  /* I have no clue what this does, except that registering it stops the 
     X server segfaulting in ProcGetPointerMapping()
     Ho Hum.
  */
}
