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
 * Copyright 1995 by Patrick Lecoanet, France. <lecoanet@cenaath.cena.dgac.fr>
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/elo2300/elo.c,v 1.11 2001/11/26 16:25:53 dawes Exp $ */

#define _elo_C_
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
#include "elo.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables
 ****************************************************************************/
static XF86ModuleVersionInfo VersionRec =
{
	"elo2300",
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

static const char *default_options[] =
{
	"BaudRate", "9600",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "None",
	"Vmin", "10",
	"Vtime", "1",
	"FlowControl", "None"
};

XF86ModuleData elo2300ModuleData = { &VersionRec, SetupProc, TearDownProc };

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static void
TearDownProc( pointer p )
{
	LocalDevicePtr local = (LocalDevicePtr) p;
	EloPrivatePtr priv = (EloPrivatePtr) local->private;

	DeviceOff (local->dev);

	xf86CloseSerial (local->fd);
	XisbFree (priv->buffer);
	xfree (priv);
	xfree (local->name);
	xfree (local);
}

static pointer
SetupProc(	pointer module,
			pointer options,
			int *errmaj,
			int *errmin )
{
	LocalDevicePtr local = xcalloc (1, sizeof (LocalDeviceRec));
	EloPrivatePtr priv = xcalloc (1, sizeof (EloPrivateRec));
	pointer		defaults,
				merged;
	char *s;


	if ((!local) || (!priv))
		goto SetupProc_fail;

	defaults = xf86OptionListCreate( default_options,
				  (sizeof (default_options) / sizeof (default_options[0])), 0);

	merged = xf86OptionListMerge( defaults, options );

	xf86OptionListReport( merged );

	local->fd = xf86OpenSerial (merged);
	if (local->fd == -1)
	{
		xf86ErrorF ("ELO 2300 driver unable to open device\n");
		*errmaj = LDR_NOPORTOPEN;
		*errmin = xf86GetErrno ();
		goto SetupProc_fail;
	}
	xf86ErrorFVerb( 6, "tty port opened successfully\n" );

	priv->min_x = xf86SetIntOption( merged, "MinX", 0 );
	priv->max_x = xf86SetIntOption( merged, "MaxX", 1000 );
	priv->min_y = xf86SetIntOption( merged, "MinY", 0 );
	priv->max_y = xf86SetIntOption( merged, "MaxY", 1000 );
	priv->untouch_delay = xf86SetIntOption( merged, "UntouchDelay", ELO_UNTOUCH_DELAY );
	priv->report_delay = xf86SetIntOption( merged, "ReportDelay", ELO_REPORT_DELAY );
	priv->screen_num = xf86SetIntOption( merged, "ScreenNumber", 0 );
	priv->button_number = xf86SetIntOption( merged, "ButtonNumber", 1 );
	priv->button_threshold = xf86SetIntOption( merged, "ButtonThreshold", 128 );

	s = xf86FindOptionValue (merged, "ReportingMode");
	if ((s) && (xf86NameCmp (s, "raw") == 0))
		priv->reporting_mode = TS_Raw;
	else
		priv->reporting_mode = TS_Scaled;

	priv->buffer = XisbNew (local->fd, 200);
	priv->button_down = FALSE;

	DBG (9, XisbTrace (priv->buffer, 1));

	EloNewPacket (priv);
	if (QueryHardware (priv, errmaj, errmin) != Success)
	{
		xf86ErrorF ("Unable to query/initialize Elographics 2300 hardware.\n");
		goto SetupProc_fail;
	}

	/* this results in an xstrdup that must be freed later */
	local->name = xf86SetStrOption( merged, "DeviceName", "ELOGraphics 2300 TouchScreen" );
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
	local->history_size = xf86SetIntOption( merged, "HistorySize", 0 );

	xf86AddLocalDevice (local, merged);

	/* prepare to process touch packets */
	EloNewPacket (priv);
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
DeviceControl(	DeviceIntPtr dev,
				int mode )
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
DeviceOn( DeviceIntPtr dev )
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;

	AddEnabledDevice (local->fd);
	dev->public.on = TRUE;
	return (Success);
}

static Bool
DeviceOff( DeviceIntPtr dev )
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;

	RemoveEnabledDevice (local->fd);
	dev->public.on = FALSE;
	return (Success);
}

static Bool
DeviceClose( DeviceIntPtr dev )
{
	return (Success);
}

static Bool
DeviceInit( DeviceIntPtr dev )
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	EloPrivatePtr priv = (EloPrivatePtr) (local->private);
	unsigned char map[] =
	{0, 1};

	/* 
	 * these have to be here instead of in the SetupProc, because when the
	 * SetupProc is run and server startup, screenInfo is not setup yet
	 */
	priv->screen_width = screenInfo.screens[priv->screen_num]->width;
	priv->screen_height = screenInfo.screens[priv->screen_num]->height;

	/* 
	 * Device reports button press for up to 1 button.
	 */
	if (InitButtonClassDeviceStruct (dev, 1, map) == FALSE)
	{
		ErrorF ("Unable to allocate Elographics touchscreen ButtonClassDeviceStruct\n");
		return !Success;
	}

	/* 
	 * Device reports motions on 2 axes in absolute coordinates.
	 * Device may reports touch pressure on the 3rd axis.
	 */
	if (InitValuatorClassDeviceStruct (dev, priv->axes, xf86GetMotionEvents,
									local->history_size, Absolute) == FALSE)
	{
		ErrorF ("Unable to allocate Elographics touchscreen ValuatorClassDeviceStruct\n");
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
		/* 3rd axis is pressure */
		if (priv->axes == 3)
		{
			InitValuatorAxisStruct (dev, 2, 0, 255,
									255,
									0 /* min_res */ ,
									255 /* max_res */ );
		}
	}

	if (InitProximityClassDeviceStruct (dev) == FALSE)
	{
		ErrorF ("Unable to allocate Elographics touchscreen ProximityClassDeviceStruct\n");
		return !Success;
	}

	/* 
	 * Allocate the motion events buffer.
	 */
	xf86MotionHistoryAllocate (local);
	return (Success);
}

static void
ReadInput( LocalDevicePtr local )
{
	int x, y, z;
	int state;
	EloPrivatePtr priv = (EloPrivatePtr) (local->private);

	/* 
	 * set blocking to -1 on the first call because we know there is data to
	 * read. Xisb automatically clears it after one successful read so that
	 * succeeding reads are preceeded buy a select with a 0 timeout to prevent
	 * read from blocking indefinately.
	 */
	XisbBlockDuration (priv->buffer, -1);
	while (EloGetPacket (priv) == Success)
	{
		if (priv->packet[1] == ELO_TOUCH)
		{
			x = WORD_ASSEMBLY (priv->packet[3], priv->packet[4]);
			y = WORD_ASSEMBLY (priv->packet[5], priv->packet[6]);
			z = WORD_ASSEMBLY (priv->packet[7], priv->packet[8]);
			state = priv->packet[2] & 0x07;

			if (priv->reporting_mode == TS_Scaled)
			{
				x = xf86ScaleAxis (x, 0, priv->screen_width, priv->min_x,
								   priv->max_x);
				y = xf86ScaleAxis (y, 0, priv->screen_height, priv->min_y,
								   priv->max_y);
			}
			xf86XInputSetScreen (local, priv->screen_num, x, y);
			if (state == ELO_PRESS)
			{
				if (priv->axes == 3)
					xf86PostProximityEvent (local->dev, 1, 0, 3, x, y, z);
				else
					xf86PostProximityEvent (local->dev, 1, 0, 2, x, y);
			}

			/* 
			 * Send events.
			 *
			 * We *must* generate a motion before a button change if pointer
			 * location has changed as DIX assumes this. This is why we always
			 * emit a motion, regardless of the kind of packet processed.
			 */
			if (priv->axes == 3)
				xf86PostMotionEvent (local->dev, TRUE, 0, 3, x, y, z);
			else
				xf86PostMotionEvent (local->dev, TRUE, 0, 2, x, y);

			/* 
			 * Emit a button press or release.
			 */
			if ((z > priv->button_threshold) && (!priv->button_down))
			{
				if (priv->axes == 3)
				{
					xf86PostButtonEvent (local->dev, TRUE,
									 priv->button_number, 1, 0, 3, x, y, z);
				}
				else
				{
					xf86PostButtonEvent (local->dev, TRUE,
										 priv->button_number, 1, 0, 2, x, y);
				}
				priv->button_down = TRUE;
			}
			/* 
			 * if button_threshold is 0, there may never be a STREAMING
			 * event with a z <= 0 before there is an untouch (RELEASE)
			 * event
			 */
			if (((z <= priv->button_threshold) || (state == ELO_RELEASE))
				&& (priv->button_down))
			{
				if (priv->axes == 3)
				{
					xf86PostButtonEvent (local->dev, TRUE,
									 priv->button_number, 0, 0, 3, x, y, z);
				}
				else
				{
					xf86PostButtonEvent (local->dev, TRUE,
										 priv->button_number, 0, 0, 2, x, y);
				}
				priv->button_down = FALSE;
			}
			/* 
			 * the untouch should always come after the button release
			 */
			if (state == ELO_RELEASE)
			{
				if (priv->axes == 3)
					xf86PostProximityEvent (local->dev, 0, 0, 3, x, y, z);
				else
					xf86PostProximityEvent (local->dev, 0, 0, 2, x, y);
			}

			xf86ErrorFVerb (3, "TouchScreen: x(%d), y(%d), %s\n",
							x, y,
							(state == ELO_PRESS) ? "Press" :
						  ((state == ELO_RELEASE) ? "Release" : "Stream"));
		}
	}
}

static int
ControlProc( LocalDevicePtr local,
			 xDeviceCtl * control )
{
	xDeviceTSCalibrationCtl *c = (xDeviceTSCalibrationCtl *) control;
	EloPrivatePtr priv = (EloPrivatePtr) (local->private);

	priv->min_x = c->min_x;
	priv->max_x = c->max_x;
	priv->min_y = c->min_y;
	priv->max_y = c->max_y;
	/* 
	 * if we have real Z values, set the threshold, otherwise stick with
	 * the built-in default.
	 */
	if (priv->axes == 3)
		priv->button_threshold = c->button_threshold;
	return (Success);
}

static void
CloseProc( LocalDevicePtr local )
{
}

static int
SwitchMode(	ClientPtr client,
			DeviceIntPtr dev,
			int mode )
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	EloPrivatePtr priv = (EloPrivatePtr) (local->private);
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
ConvertProc( LocalDevicePtr local,
			 int first,
			 int num,
			 int v0,
			 int v1,
			 int v2,
			 int v3,
			 int v4,
			 int v5,
			 int *x,
			 int *y )
{
	EloPrivatePtr priv = (EloPrivatePtr) (local->private);

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
QueryHardware(	EloPrivatePtr priv,
				int *errmaj,
				int *errmin )
{
	unsigned char req[ELO_PACKET_SIZE];

	memset (req, 0, ELO_PACKET_SIZE);
	req[1] = tolower (ELO_ID);
	if (EloSendQuery (req, priv) == Success)
	{
		/* 
		 * indicates hardware reports actual z (pressure) values
		 */
		if (priv->packet[4] & 0x80)
			priv->axes = 3;
		else
			priv->axes = 2;

		EloPrintIdent( priv->packet );
		if (EloWaitAck (priv) != Success)
		{
			*errmaj = LDR_NOHARDWARE;
			return (!Success);
		}
	}
	else
	{
		*errmaj = LDR_NOHARDWARE;
		return (!Success);
	}
	/* 
	 * Set the operating mode: Stream, no scaling, no calibration,
	 * no range checking, no trim, tracking enabled.
	 */
	memset (req, 0, ELO_PACKET_SIZE);
	req[1] = ELO_MODE;
	req[3] = ELO_TOUCH_MODE | ELO_STREAM_MODE | ELO_UNTOUCH_MODE |
		ELO_ENABLE_Z_MODE;
	req[4] = ELO_TRACKING_MODE;
	if (EloSendControl (req, priv) != Success)
	{
		ErrorF ("Unable to change Elographics touchscreen modes\n");
		*errmaj = LDR_NOHARDWARE;
		return (!Success);
	}

	/* 
	 * Set the touch reports timings from configuration data.
	 */
	memset (req, 0, ELO_PACKET_SIZE);
	req[1] = ELO_REPORT;
	req[2] = priv->untouch_delay;
	req[3] = priv->report_delay;
	if (EloSendControl (req, priv) != Success)
	{
		ErrorF ("Unable to change Elographics touchscreen reports timings\n");
		*errmaj = LDR_NOHARDWARE;
		return (!Success);

	}
	return (Success);
}

/* 
 ***************************************************************************
 *
 * EloSendControl --
 *  Emit a control command to the controller and wait for acknowledge.
 *
 *  Returns Success if acknowledge received and reported no error.
 *
 ***************************************************************************
 */
static Bool
EloSendControl(	unsigned char *control,
				EloPrivatePtr priv )
{
	if (EloSendPacket (control, priv->buffer) == Success)
	{
		return EloWaitAck (priv);
	}
	else
		return !Success;
}

/* 
 ***************************************************************************
 *
 * EloSendQuery --
 *  Emit a query to the controller and blocks until the reply and
 *  the acknowledge are read.
 *
 *  The reply is left in reply. The function returns Success if the
 *  reply is valid and !Success otherwise.
 *
 ***************************************************************************
 */
static Bool
EloSendQuery( unsigned char *request,
			  EloPrivatePtr priv )
{
	Bool ok;

	if (EloSendPacket (request, priv->buffer) == Success)
	{
		ok = EloWaitReply (toupper (request[1]), priv);
		return ok;
	}
	else
		return !Success;
}

/* 
 ***************************************************************************
 *
 * EloSendPacket --
 *  Emit an height bytes packet to the controller.
 *  The function expects a valid buffer containing the
 *  command to be sent to the controller. It fills it with the
 *  leading sync character an the trailing checksum byte.
 *
 ***************************************************************************
 */
static Bool
EloSendPacket(	unsigned char *packet,
				XISBuffer * b )
{
	int i, result;
	int sum = ELO_INIT_CHECKSUM;

	packet[0] = ELO_SYNC_BYTE;
	for (i = 0; i < ELO_PACKET_SIZE - 1; i++)
	{
		sum += packet[i];
		sum &= 0xFF;
	}
	packet[ELO_PACKET_SIZE - 1] = sum;

	result = XisbWrite (b, packet, ELO_PACKET_SIZE);
	if (result != ELO_PACKET_SIZE)
	{
		ErrorF ("System error while sending to Elographics touchscreen.\n");
		return !Success;
	}
	else
		return Success;
}

/* 
 ***************************************************************************
 *
 * EloWaitReply --
 *  It is assumed that the reply will be in the few next bytes
 *  read and will be available very soon after the query post. if
 *  these two asumptions are not met, there are chances that the server
 *  will be stuck for a while.
 *  The reply type need to match parameter 'type'.
 *  The reply is left in reply. The function returns Success if the
 *  reply is valid and !Success otherwise.
 *
 ***************************************************************************
 */

static Bool
EloWaitReply( unsigned char type,
			  EloPrivatePtr priv )
{
	Bool ok;
	int wrong, empty;

	xf86ErrorFVerb( 4, "Waiting for a '%c' reply\n", type );
	wrong = ELO_MAX_WRONG_PACKETS;
	empty = ELO_MAX_EMPTY_PACKETS;
	do
	{
		ok = !Success;

		/* 
		 * Wait half a second for the reply. The fuse counts down each
		 * timeout and each wrong packet.
		 */
		xf86ErrorFVerb( 4, "Waiting %d ms for data from port\n",
						ELO_MAX_WAIT / 1000 );
		EloNewPacket (priv);
		XisbBlockDuration (priv->buffer, ELO_MAX_WAIT);
		ok = EloGetPacket (priv);
		if (ok != Success)
			empty--;
		if ((ok == Success) && (priv->packet[1] != type))
		{
			xf86ErrorFVerb( 2, "Wrong reply received\n" );
			ok = !Success;
			wrong--;
		}
	}
	while (ok != Success && wrong && empty);

	return ok;
}
/* 
 * prepare to read a new packet regardless of the state of the packet in
 * progress
 */
static void
EloNewPacket( EloPrivatePtr priv )
{
	priv->packeti = 0;
	priv->lex_mode = elo_normal;
	priv->checksum = 0;
}

/* 
 ***************************************************************************
 *
 * EloGetPacket --
 *  Read a packet from the port. Try to synchronize with start of
 *  packet and compute checksum.
 *      The packet structure read by this function is as follow:
 *      Byte 0 : ELO_SYNC_BYTE
 *      Byte 1
 *      ...
 *      Byte 8 : packet data
 *      Byte 9 : checksum of bytes 0 to 8
 *
 *  This function returns if a valid packet has been assembled in
 *  buffer or if no more data is available.
 *
 *  Returns Success if a packet is successfully assembled including
 *  testing checksum. If a packet checksum is incorrect, it is discarded.
 *  Bytes preceding the ELO_SYNC_BYTE are also discarded.
 *  Returns !Success if out of data while reading. The start of the
 *  partially assembled packet is left in packet, buffer_p and
 *  checksum reflect the current state of assembly.
 *
 ***************************************************************************
 */
static Bool
EloGetPacket( EloPrivatePtr priv )
{
	int count = 0;
	int c;

	while ((c = XisbRead (priv->buffer)) >= 0)
	{
		if (priv->lex_mode != elo_checksum)
			priv->checksum += c;
		if (count++ > 50)
		{
			EloNewPacket (priv);
			return (!Success);
		}

		switch (priv->lex_mode)
		{
		case elo_normal:
			if (c == ELO_SYNC_BYTE)
			{
				priv->packet[priv->packeti++] = (unsigned char) c;
				priv->checksum = ELO_INIT_CHECKSUM + c;
				priv->lex_mode = elo_body;
			}
			break;

		case elo_body:
			if (priv->packeti < 9)
				priv->packet[priv->packeti++] = (unsigned char) c;
			if (priv->packeti == 9)
				priv->lex_mode = elo_checksum;
			break;

		case elo_checksum:

			if (c != priv->checksum)
			{
				xf86ErrorFVerb( 4, "Checksum mismatch. Read %d calculated %d\nPacket discarded.\n",
								   c, priv->checksum );
				EloNewPacket (priv);
			}
			else
			{
				EloNewPacket (priv);
				xf86ErrorFVerb( 5, "got a good packet from ELO touchscreen\n" );
				return (Success);
			}
			break;
		}
	}
	return (!Success);
}

/* 
 ***************************************************************************
 *
 * EloWaitAck --
 *  Wait for an acknowledge from the controller. Returns Success if
 *  acknowledge received and reported no errors.
 *
 ***************************************************************************
 */
static Bool
EloWaitAck( EloPrivatePtr priv )
{
	int i, nb_errors;

	if (EloWaitReply (ELO_ACK, priv) == Success)
	{
		for (i = 0, nb_errors = 0; i < 4; i++)
			if (priv->packet[2 + i] != '0')
				nb_errors++;
		if (nb_errors != 0)
		{
			xf86ErrorFVerb( 2, "Elographics acknowledge packet reports %d errors\n",
							nb_errors );
		}
		return Success;
		/* return (nb_errors < 4) ? Success : !Success; */
	}
	else
		return !Success;
}

/* 
 ***************************************************************************
 *
 * EloPrintIdent --
 *  Print type of touchscreen and features on controller board.
 *
 ***************************************************************************
 */
static void
EloPrintIdent( unsigned char *packet )
{
	xf86Msg( X_PROBED, " Elographics touchscreen is a " );
	switch (packet[2])
	{
	case '0':
		xf86ErrorF( "AccuTouch" );
		break;
	case '1':
		xf86ErrorF( "DuraTouch" );
		break;
	case '2':
		xf86ErrorF( "Intellitouch" );
		break;
	}
	xf86ErrorF( ", connected through a " );
	switch (packet[3])
	{
	case '0':
		xf86ErrorF( "serial link.\n" );
		break;
	case '1':
		xf86ErrorF( "ISA-Bus card.\n" );
		break;
	case '2':
		xf86ErrorF( "Micro Channel card.\n");
		break;
	}
	xf86Msg( X_PROBED, " The controller is a model " );
	if (packet[8] & 1)
		xf86ErrorF( "E271-2210" );
	else
		xf86ErrorF( "E271-2200" );
	xf86ErrorF( ", firmware revision %d.%d.\n", packet[6], packet[5] );

	if( packet[4] )
	{
		xf86Msg( X_PROBED, " Additional features:\n" );
		if (packet[4] & 0x10)
			xf86Msg( X_PROBED, "	External A/D converter\n" );
		if (packet[4] & 0x20)
			xf86Msg( X_PROBED, "	32K RAM\n" );
		if (packet[4] & 0x40)
			xf86Msg( X_PROBED, "	RAM onboard\n" );
		if (packet[4] & 0x80)
			xf86Msg( X_PROBED, "	Z axis active\n" );
		xf86ErrorF( "\n" );
	}
}
