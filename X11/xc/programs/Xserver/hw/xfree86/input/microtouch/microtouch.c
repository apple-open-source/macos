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
 * Based, in part, on code with the following copyright notice:
 *
 * Copyright 1996 by Patrick Lecoanet, France. <lecoanet@cenaath.cena.dgac.fr>       
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Patrick  Lecoanet not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Patrick Lecoanet   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * PATRICK LECOANET DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT  SHALL PATRICK LECOANET BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/microtouch/microtouch.c,v 1.12 2000/08/11 19:10:45 dawes Exp $ */

#define _microtouch_C_
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
#include "microtouch.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables
 ****************************************************************************/

static InputInfoPtr
MuTouchPreInit(InputDriverPtr drv, IDevPtr dev, int flags);


InputDriverRec MICROTOUCH = {
  1,
  "microtouch",
  NULL,
  MuTouchPreInit,
  /*MuTouchUnInit*/ NULL,
  NULL,
  0
};

#ifdef XFree86LOADER

static XF86ModuleVersionInfo VersionRec =
{
	"microtouch",
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
	"RemoveEnabledDevice",
	"Xcalloc",
	"Xfree",
	"XisbBlockDuration",
	"XisbFree",
	"XisbNew",
	"XisbRead",
	"XisbTrace",
	"XisbWrite",
	"screenInfo",
	"xf86AddInputDriver",
	"xf86AllocateInput",
	"xf86CloseSerial",
	"xf86CollectInputOptions",
	"xf86ErrorF",
	"xf86ErrorFVerb",
	"xf86FindOptionValue",
	"xf86FlushInput",
	"xf86GetMotionEvents",
	"xf86GetVerbosity",
	"xf86LoaderReqSymLists",
	"xf86MotionHistoryAllocate",
	"xf86Msg",
	"xf86NameCmp",
	"xf86OpenSerial",
	"xf86OptionListCreate",
	"xf86OptionListFree",
	"xf86OptionListReport",
	"xf86PostButtonEvent",
	"xf86PostMotionEvent",
	"xf86PostProximityEvent",
	"xf86ProcessCommonOptions",
	"xf86ScaleAxis",
	"xf86SetIntOption",
	"xf86SetSerial",
	"xf86SetStrOption",
	"xf86XInputSetScreen",
	"xf86XInputSetSendCoreEvents",
	"xf86memset",
	"xf86sscanf",
	"xf86strcmp",
	"xf86strlen",
	"xf86strncmp",
	"xf86strncpy",
        NULL
};

static pointer
SetupProc(	pointer module,
			pointer options,
			int *errmaj,
			int *errmin )
{
        xf86LoaderReqSymLists(reqSymbols, NULL);
	xf86AddInputDriver(&MICROTOUCH, module, 0);
	return (pointer) 1;
}

XF86ModuleData microtouchModuleData = {&VersionRec, &SetupProc, NULL };

#endif /* XFree86LOADER */


static const char *default_options[] =
{
	"BaudRate", "9600",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "None",
	"Vmin", "5",
	"Vtime", "1",
	"FlowControl", "None"
};
static const char *fallback_options[] =
{
	"BaudRate", "9600",
	"StopBits", "2",
	"DataBits", "7",
	"Parity", "None",
	"Vmin", "1",
	"Vtime", "1",
	"FlowControl", "None"
};


/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/



static InputInfoPtr
MuTouchPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	LocalDevicePtr local;
	MuTPrivatePtr priv;

	char *s;

	priv = xcalloc (1, sizeof (MuTPrivateRec));
	if (!priv)
		return NULL;

	local = xf86AllocateInput(drv, 0);
	if (!local) {
		xfree(priv);
		return NULL;
	}

	local->type_name = XI_TOUCHSCREEN;
	local->device_control = DeviceControl;
	local->read_input = ReadInput;
	local->control_proc = ControlProc;
	local->close_proc = CloseProc;
	local->switch_mode = SwitchMode;
	local->conversion_proc = ConvertProc;
	local->dev = NULL;
	local->private = priv;
	local->private_flags = 0;
	local->flags = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
	local->conf_idev = dev;

	xf86CollectInputOptions(local, default_options, NULL);

	xf86OptionListReport(local->options);

	local->fd = xf86OpenSerial (local->options);
	if (local->fd == -1)
	{
		ErrorF ("MicroTouch driver unable to open device\n");
		goto SetupProc_fail;
	}
	xf86ErrorFVerb( 6, "tty port opened successfully\n" );

	priv->min_x = xf86SetIntOption( local->options, "MinX", 0 );
	priv->max_x = xf86SetIntOption( local->options, "MaxX", 1000 );
	priv->min_y = xf86SetIntOption( local->options, "MinY", 0 );
	priv->max_y = xf86SetIntOption( local->options, "MaxY", 1000 );
	priv->screen_num = xf86SetIntOption( local->options, "ScreenNumber", 0 );
	priv->button_number = xf86SetIntOption( local->options, "ButtonNumber", 1 );

	s = xf86FindOptionValue (local->options, "ReportingMode");
	if ((s) && (xf86NameCmp (s, "raw") == 0))
		priv->reporting_mode = TS_Raw;
	else
		priv->reporting_mode = TS_Scaled;

	priv->buffer = XisbNew (local->fd, 200);
	priv->proximity = FALSE;
	priv->button_down = FALSE;

	DBG (9, XisbTrace (priv->buffer, 1));

	MuTNewPacket (priv);
	if (QueryHardware(local) != Success)
	{
		ErrorF ("Unable to query/initialize MicroTouch hardware.\n");
		goto SetupProc_fail;
	}

	local->history_size = xf86SetIntOption( local->options, "HistorySize", 0 );


	/* prepare to process touch packets */
	MuTNewPacket (priv);

	/* this results in an xstrdup that must be freed later */
	local->name = xf86SetStrOption( local->options, "DeviceName", "MicroTouch TouchScreen" );
	xf86ProcessCommonOptions(local, local->options);
	local->flags |= XI86_CONFIGURED;

	if (local->fd != -1)
	{ 
		RemoveEnabledDevice (local->fd);
		if (priv->buffer)
		{
			XisbFree(priv->buffer);
			priv->buffer = NULL;
		}
		xf86CloseSerial(local->fd);
	}
	RemoveEnabledDevice (local->fd);
	local->fd = -1;
	return (local);

  SetupProc_fail:
	if ((local) && (local->fd))
		xf86CloseSerial (local->fd);
	if ((local) && (local->name))
		xfree (local->name);

	if ((priv) && (priv->buffer))
		XisbFree (priv->buffer);
	if (priv)
		xfree (priv);
	return (local);
}

static Bool
DeviceControl (DeviceIntPtr dev,
			   int mode)
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
	case DEVICE_CLOSE:
		RetValue = DeviceOff( dev );
		break;
	default:
		RetValue = BadValue;
	}

	return( RetValue );
}

static Bool
DeviceOn (DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	MuTPrivatePtr priv = (MuTPrivatePtr) (local->private);

	local->fd = xf86OpenSerial(local->options);
	if (local->fd == -1)
	{
		xf86Msg(X_WARNING, "%s: cannot open input device\n", local->name);
		return (!Success);
	}

	priv->buffer = XisbNew(local->fd, 64);
	if (!priv->buffer) 
		{
			xf86CloseSerial(local->fd);
			local->fd = -1;
			return (!Success);
		}

	xf86FlushInput(local->fd);
	AddEnabledDevice (local->fd);
	dev->public.on = TRUE;
	return (Success);
}

static Bool
DeviceOff (DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	MuTPrivatePtr priv = (MuTPrivatePtr) (local->private);

	if (local->fd != -1)
        { 
	RemoveEnabledDevice (local->fd);
		if (priv->buffer)
		{
			XisbFree(priv->buffer);
			priv->buffer = NULL;
}
		xf86CloseSerial(local->fd);
	}

	RemoveEnabledDevice (local->fd);
	dev->public.on = FALSE;
	return (Success);
}

static Bool
DeviceInit (DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	MuTPrivatePtr priv = (MuTPrivatePtr) (local->private);
	unsigned char map[] =
	{0, 1};

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
		ErrorF ("Unable to allocate MicroTouch touchscreen ButtonClassDeviceStruct\n");
		return !Success;
	}

	/* 
	 * Device reports motions on 2 axes in absolute coordinates.
	 * Axes min and max values are reported in raw coordinates.
	 */
	if (InitValuatorClassDeviceStruct (dev, 2, xf86GetMotionEvents,
									local->history_size, Absolute) == FALSE)
	{
		ErrorF ("Unable to allocate MicroTouch touchscreen ValuatorClassDeviceStruct\n");
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
		ErrorF ("Unable to allocate MicroTouch touchscreen ProximityClassDeviceStruct\n");
		return !Success;
	}

	/* 
	 * Allocate the motion events buffer.
	 */
	xf86MotionHistoryAllocate (local);
	return (Success);
}

#define WORD_ASSEMBLY(byte1, byte2) (((byte2) << 7) | (byte1))

static void
ReadInput (LocalDevicePtr local)
{
	int x, y;
	int type;
	MuTPrivatePtr priv = (MuTPrivatePtr) (local->private);

	/* 
	 * set blocking to -1 on the first call because we know there is data to
	 * read. Xisb automatically clears it after one successful read so that
	 * succeeding reads are preceeded buy a select with a 0 timeout to prevent
	 * read from blocking indefinately.
	 */
	XisbBlockDuration (priv->buffer, -1);
	while (MuTGetPacket (priv) == Success)
	{
		type = priv->packet[0];
		x = WORD_ASSEMBLY (priv->packet[1], priv->packet[2]);
		y = WORD_ASSEMBLY (priv->packet[3], priv->packet[4]);

		if (priv->reporting_mode == TS_Scaled)
		{
			x = xf86ScaleAxis (x, 0, priv->screen_width, priv->min_x,
							   priv->max_x);
			y = xf86ScaleAxis (y, 0, priv->screen_height, priv->min_y,
							   priv->max_y);
		}

		xf86XInputSetScreen (local, priv->screen_num, x, y);

		if ((priv->proximity == FALSE) && (type & MuT_CONTACT))
		{
			priv->proximity = TRUE;
			xf86PostProximityEvent (local->dev, 1, 0, 2, x, y);
		}

		/* 
		 * Send events.
		 *
		 * We *must* generate a motion before a button change if pointer
		 * location has changed as DIX assumes this. This is why we always
		 * emit a motion, regardless of the kind of packet processed.
		 */
		xf86PostMotionEvent (local->dev, TRUE, 0, 2, x, y);

		/* 
		 * Emit a button press or release.
		 */
		if ((priv->button_down == FALSE) && (type & MuT_CONTACT))

		{
			xf86PostButtonEvent (local->dev, TRUE,
								 priv->button_number, 1, 0, 2, x, y);
			priv->button_down = TRUE;
		}
		if ((priv->button_down == TRUE) && !(type & MuT_CONTACT))
		{
			xf86PostButtonEvent (local->dev, TRUE,
								 priv->button_number, 0, 0, 2, x, y);
			priv->button_down = FALSE;
		}
		/* 
		 * the untouch should always come after the button release
		 */
		if ((priv->proximity == TRUE) && !(type & MuT_CONTACT))

		{
			priv->proximity = FALSE;
			xf86PostProximityEvent (local->dev, 0, 0, 2, x, y);
		}

		xf86ErrorFVerb( 3, "TouchScreen: x(%d), y(%d), %d %d %s\n",
						x, y, type, type & MuT_CONTACT,
						(type & MuT_CONTACT) ? "Press" : "Release" );
	}
}

static int
ControlProc (LocalDevicePtr local, xDeviceCtl * control)
{
	xDeviceTSCalibrationCtl *c = (xDeviceTSCalibrationCtl *) control;
	MuTPrivatePtr priv = (MuTPrivatePtr) (local->private);

	priv->min_x = c->min_x;
	priv->max_x = c->max_x;
	priv->min_y = c->min_y;
	priv->max_y = c->max_y;
	return (Success);
}

static void
CloseProc (LocalDevicePtr local)
{
}

static int
SwitchMode (ClientPtr client, DeviceIntPtr dev, int mode)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	MuTPrivatePtr priv = (MuTPrivatePtr) (local->private);
	if ((mode == TS_Raw) || (mode == TS_Scaled))
	{
		priv->reporting_mode = mode;
		return (Success);
	}
	else if ((mode == SendCoreEvents) || (mode == DontSendCoreEvents))
	{
		xf86XInputSetSendCoreEvents (local, (mode == SendCoreEvents));
		return (Success);
	}
	else
		return (!Success);
}

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
	MuTPrivatePtr priv = (MuTPrivatePtr) (local->private);

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

static Bool
xf86MuTSendCommand (unsigned char *type, MuTPrivatePtr priv)
{
	int r;
	int retries = MuT_RETRIES;

	while (retries--)
	{
		if (xf86MuTSendPacket (type, strlen ( (char *)type), priv) != Success)
			continue;
		r = xf86MuTWaitReply ( (unsigned char *)MuT_OK, priv);
		if (r == ACK)
			return (TRUE);
		else if (r == NACK)
			return (FALSE);
	}
	return (FALSE);
}

/* 
 * The microtouch SMT3 factory default is 72N, but the recommended operating
 * mode is 81N. This code first tries 81N, but if that fails it switches to
 * 72N and puts the controller in 81N before proceeding
 */
static Bool
QueryHardware (LocalDevicePtr local)
{
        MuTPrivatePtr priv = (MuTPrivatePtr) local->private;
	pointer	fallback;
	Bool ret = Success;
	int cs7 = FALSE;

	fallback = xf86OptionListCreate (fallback_options,
				(sizeof (fallback_options) / sizeof (fallback_options[0])), 0);

	priv->cs7flag = TRUE;
	if (!xf86MuTSendCommand ( (unsigned char *)MuT_RESET, priv))
	{
		xf86ErrorFVerb( 5, 
			  "Switching Com Parameters to CS7, 2 stop bits, no parity\n" );
		xf86SetSerial (priv->buffer->fd, fallback);
		cs7 = TRUE;
		if (!xf86MuTSendCommand ( (unsigned char *)MuT_RESET, priv))
		{
			ret = !Success;
			goto done;
		}
	}
	if (!xf86MuTSendCommand ( (unsigned char *)MuT_ABDISABLE, priv))
	{
		ret = !Success;
		goto done;
	}
	if (!xf86MuTSendCommand ( (unsigned char *)MuT_SETRATE, priv))
	{
		ret = !Success;
		goto done;
	}
	if (cs7)
	{
		xf86ErrorFVerb( 5, 
		  "Switching Com Parameters back to CS8, 1 stop bit, no parity\n" );
		xf86SetSerial (priv->buffer->fd, local->options);
	}
	priv->cs7flag = FALSE;
	if (!xf86MuTSendCommand ( (unsigned char *)MuT_FORMAT_TABLET, priv))
	{
		ret = !Success;
		goto done;
	}
	if (!xf86MuTSendCommand ( (unsigned char *)MuT_MODE_STREAM, priv))
	{
		ret = !Success;
		goto done;
	}
	if (!xf86MuTSendCommand ( (unsigned char *)MuT_PARAM_LOCK, priv))
	{
		ret = !Success;
		goto done;
	}
	if( 1 )		/* Was:   if (xf86Verbose), but can't do that in 3.9N...	*/
	{
		if (xf86MuTSendPacket ( (unsigned char *)MuT_OUTPUT_IDENT, strlen (MuT_OUTPUT_IDENT),
							   priv) == Success)
		{
			if (MuTGetPacket (priv) == Success)
				xf86MuTPrintIdent (priv->packet);
		}

		/* some microtouch controllers support one command, some support the
		 * * other. If the first one get's a NACK, try the second. They both
		 * * return the same packet. */
		if (xf86MuTSendPacket ( (unsigned char *)MuT_UNIT_VERIFY, strlen (MuT_UNIT_VERIFY),
							   priv) == Success)
		{
			if ((MuTGetPacket (priv) == Success) &&
				(strcmp ( (char *)&(priv->packet[1]), MuT_ERROR) == 0))
			{
				if (xf86MuTSendPacket ( (unsigned char *)MuT_UNIT_TYPE,
								   strlen (MuT_UNIT_TYPE), priv) == Success)
				{
					if ((MuTGetPacket (priv) != Success))
					{
						ret = !Success;
						goto done;
					}
				}
			}
			ret = xf86MuTPrintHwStatus (priv->packet);
		}
	}

  done:
	xf86OptionListFree (fallback);

	return (ret);
}

static void
MuTNewPacket (MuTPrivatePtr priv)
{
	priv->lex_mode = microtouch_normal;
	priv->packeti = 0;
	priv->binary_pkt = FALSE;
}

/* 
 ***************************************************************************
 *
 * xf86MuTSendPacket --
 *  Emit a variable length packet to the controller.
 *  The function expects a valid buffer containing the
 *  command to be sent to the controller.  The command
 *  size is in len
 *  The buffer is filled with the leading and trailing
 *  character before sending.
 *
 ***************************************************************************
 */
static Bool
xf86MuTSendPacket (unsigned char *type, int len, MuTPrivatePtr priv)
{
	int result;
	unsigned char req[MuT_PACKET_SIZE];

	memset (req, 0, MuT_PACKET_SIZE);
	strncpy ((char *) &req[1],  (char *)type, strlen ( (char *)type));
	req[0] = MuT_LEAD_BYTE;
	req[len + 1] = MuT_TRAIL_BYTE;

	result = XisbWrite (priv->buffer, req, len + 2);
	if (result != len + 2)
	{
		xf86ErrorFVerb( 5, "System error while sending to MicroTouch touchscreen.\n" );
		return !Success;
	}
	else
		return Success;
}

/* 
 ***************************************************************************
 *   0 ACK
 *  -1 NACK
 *  -2 timeout
 *  -3 wrong packet type
 ***************************************************************************
 */
static int
xf86MuTWaitReply (unsigned char *type, MuTPrivatePtr priv)
{
	Bool ok;
	int wrong, empty;

	wrong = MuT_MAX_WRONG_PACKETS;
	empty = MuT_MAX_EMPTY_PACKETS;
	do
	{
		ok = !Success;

		/* 
		 * Wait half a second for the reply. The fuse counts down each
		 * timeout and each wrong packet.
		 */
		xf86ErrorFVerb( 4, "Waiting %d ms for data from port\n", MuT_MAX_WAIT / 1000 );
		MuTNewPacket (priv);
		XisbBlockDuration (priv->buffer, MuT_MAX_WAIT);
		ok = MuTGetPacket (priv);
		/* 
		 * type is a NULL terminated string of 0 - 2 characters. An empty
		 * string for type indicates that any reply type is acceptable.
		 */
		if (ok != Success)
		{
			xf86ErrorFVerb( 4, "Recieved empty packet.\n" );
			empty--;
			continue;
		}
		if (ok == Success)
		{
			/* 
			 * this bit of weirdness attempts to detect an ACK from the
			 * controller when it is in 7bit mode and the computer is in 8bit
			 * mode. If we see this pattern and send a NACK here the next
			 * level up will switch to 7 bit mode and try again
			 */
			if (priv->cs7flag && (priv->packet[1] == MuT_OK7) &&
				(priv->packet[2] == '\0'))
			{
				xf86ErrorFVerb( 4, "Detected the 7 bit ACK in 8bit mode.\n" );
				return (NACK);
			}
			if (strcmp ( (char *)&(priv->packet[1]),  (char *)type) == 0)
			{
				xf86ErrorFVerb( 5, "\t\tgot an ACK\n" );
				return (ACK);
			}
			else if (strcmp ( (char *)&(priv->packet[1]), MuT_ERROR) == 0)
			{
				xf86ErrorFVerb( 5, "\t\tgot a NACK\n" );
				return (NACK);
			}
			else
			{
				xf86ErrorFVerb( 2, "Wrong reply received\n" );
				ok = !Success;
				wrong--;
			}
		}
	}
	while (ok != Success && wrong && empty);

	if (wrong)
		return (TIMEOUT);
	else
		return (WRONG_PACKET);
}

static Bool
MuTGetPacket (MuTPrivatePtr priv)
{
	int count = 0;
	int c;

	while ((c = XisbRead (priv->buffer)) >= 0)
	{
		if (count++ > 100)
		{
			MuTNewPacket (priv);
			return (!Success);
		}

		switch (priv->lex_mode)
		{
		case microtouch_normal:
			if ((c == MuT_LEAD_BYTE) ||
				(priv->cs7flag && ((c & 0x7f) == MuT_LEAD_BYTE)))
			{
				xf86ErrorFVerb( 8, "Saw MuT_LEAD_BYTE\n" );
				priv->packet[priv->packeti++] = (unsigned char) c;
				priv->lex_mode = microtouch_body;
			}
			/* 
			 * binary touch packets do not have LEAD_BYTE or TRAIL_BYTE
			 * Instead, only the first byte has the 8th bit set.
			 */
			if (c & 0x80)
			{
				xf86ErrorFVerb( 8, "Saw BINARY start\n" );
				priv->packet[priv->packeti++] = (unsigned char) c;
				priv->lex_mode = mtouch_binary;
				priv->bin_byte = 0;
			}
			break;

		case mtouch_binary:
			priv->packet[priv->packeti++] = (unsigned char) c;
			priv->bin_byte++;
			if (priv->bin_byte == 4)
			{
				xf86ErrorFVerb( 8, "got a good BINARY packet\n" );
				MuTNewPacket (priv);
				priv->binary_pkt = TRUE;
				return (Success);
			}
			break;

		case microtouch_body:
			/* 
			 * apparently a new packet can start in the middle of another
			 * packet if they host sends something at the right time to
			 * trigger it.
			 */
			if ((c == MuT_LEAD_BYTE) ||
				(priv->cs7flag && ((c & 0x7f) == MuT_LEAD_BYTE)))
			{
				priv->packeti = 0;
			}
			if ((c == MuT_TRAIL_BYTE) ||
				(priv->cs7flag && ((c & 0x7f) == MuT_TRAIL_BYTE)))
			{
				/* null terminate the packet */
				priv->packet[priv->packeti++] = '\0';
				xf86ErrorFVerb( 8, "got a good packet\n" );
				MuTNewPacket (priv);
				return (Success);
			}
			else
				priv->packet[priv->packeti++] = (unsigned char) c;
			break;

		}
	}
	return (!Success);
}

/* 
 ***************************************************************************
 *
 * xf86MuTPrintIdent --
 *  Print type of touchscreen and features on controller board.
 *
 ***************************************************************************
 */
static void
xf86MuTPrintIdent (unsigned char *packet)
{
	int vers, rev;

	if (strlen ( (char *)packet) < 6)
		return;
	xf86Msg( X_PROBED, " MicroTouch touchscreen is " );
	if (strncmp ((char *) &packet[1], MuT_TOUCH_PEN_IDENT, 2) == 0)
		xf86ErrorF( "a TouchPen.\n" );
	else if (strncmp ((char *) &packet[1], MuT_SMT3_IDENT, 2) == 0)
		xf86ErrorF( "a Serial/SMT3.\n" );
	else if (strncmp ((char *) &packet[1], MuT_GENERAL_IDENT, 2) == 0)
		xf86ErrorF( "an SMT2, SMT3V or SMT3RV.\n" );
	else
		xf86ErrorF( "Unknown Type %c%c.\n", packet[1], packet[2] );
	sscanf ((char *) &packet[3], "%2d%2d", &vers, &rev);
	xf86Msg( X_PROBED, " MicroTouch controller firmware revision is %d.%d.\n", vers, rev);
}

/* 
 ***************************************************************************
 *
 * xf86MuTPrintHwStatus --
 *  Print status of hardware. That is if the controller report errors,
 *  decode and display them.
 *
 ***************************************************************************
 */
static Bool
xf86MuTPrintHwStatus (unsigned char *packet)
{
	int i;
	int err;

	for (i = 3; i < 7; i++)
	{
		if (packet[i] == 'R')
		{
			xf86Msg( X_PROBED,
							" MicroTouch controller is a resistive type.\n" );
		}

	}
	if (packet[7] == '1')
	{
		xf86Msg( X_PROBED, 
				" MicroTouch controller reports the following errors:\n" );
		err = packet[8];
		if (err & 0x01)
			xf86ErrorF( "\tReserved\n" );
		if (err & 0x02)
			xf86ErrorF( "\tROM error. Firmware checksum verification error.\n" );
		if (err & 0x04)
			xf86ErrorF( "\tPWM error. Unable to establish PWM operating range at power-up.\n" );
		if (err & 0x08)
			xf86ErrorF( "\tNOVRAM error. The operating parameters in the controller NOVRAM are invalid.\n" );
		if (err & 0x10)
			xf86ErrorF( "\tHWD error. The controller hardware failed.\n" );
		if (err & 0x20)
			xf86ErrorF( "\tReserved\n" );
		if (err & 0x40)
			xf86ErrorF( "\tCable NOVRAM error. The linearization data in the cable NOVRAM is invalid.\n" );
		if (err & 0x80)
			xf86ErrorF( "\tNOVRAM2 error. The linearization data in the controller NOVRAM is invalid.\n" );
		return (!Success);
	}

	return (Success);
}
