/*
 * THIS SAMPLE INPUT DRIVER IS OUT OF DATE.  DO NOT USE IT AS A TEMPLATE
 * WHEN WRITING A NEW INPUT DRIVER.
 */

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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/sample/sample.c,v 1.11 2002/01/15 15:32:45 dawes Exp $ */

#define _SAMPLE_C_
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
#include "sample.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables
 ****************************************************************************/
static XF86ModuleVersionInfo VersionRec =
{
	"sample",
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
	"Vmin", "10",
	"Vtime", "1",
	"FlowControl", "None"
};

XF86ModuleData sampleModuleData = { &VersionRec, SetupProc, TearDownProc };

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/


/* 
 * The TearDownProc may have to be tailored to your device
 */
static void
TearDownProc( pointer p )
{
	LocalDevicePtr local = (LocalDevicePtr) p;
	SAMPLEPrivatePtr priv = (SAMPLEPrivatePtr) local->private;

	ErrorF ("Sample TearDownProc Called\n");

	DeviceOff (local->dev);

	xf86CloseSerial (local->fd);
	XisbFree (priv->buffer);
	xfree (priv);
	xfree (local->name);
	xfree (local);
}

/* 
 * The Setup Proc will have to be tailored to your device
 */
static pointer
SetupProc(	pointer module,
			pointer options,
			int *errmaj,
			int *errmin )
{
	LocalDevicePtr local = xcalloc (1, sizeof (LocalDeviceRec));
	SAMPLEPrivatePtr priv = xcalloc (1, sizeof (SAMPLEPrivateRec));
	pointer	defaults,
			merged;
	char *s;

	ErrorF ("Sample SetupProc called\n");
	if ((!local) || (!priv))
		goto SetupProc_fail;

	defaults = xf86OptionListCreate (default_options,
				  (sizeof (default_options) / sizeof (default_options[0])), 0);
	merged = xf86OptionListMerge (defaults, options);

	xf86OptionListReport( merged );

	local->fd = xf86OpenSerial (merged);
	if (local->fd == -1)
	{
		ErrorF ("SAMPLE driver unable to open device\n");
		*errmaj = LDR_NOPORTOPEN;
		*errmin = xf86GetErrno ();
		goto SetupProc_fail;
	}

	/* 
	 * Process the options for your device like this
	 */
	priv->min_x = xf86SetIntOption( merged, "MinX", 0 );
	priv->max_x = xf86SetIntOption( merged, "MaxX", 1000 );
	priv->min_y = xf86SetIntOption( merged, "MinY", 0 );
	priv->max_y = xf86SetIntOption( merged, "MaxY", 1000 );
	priv->untouch_delay = xf86SetIntOption( merged, "UntouchDelay", 10 );
	priv->report_delay = xf86SetIntOption( merged, "ReportDelay", 40 );
	priv->screen_num = xf86SetIntOption( merged, "ScreenNumber", 0 );
	priv->button_number = xf86SetIntOption( merged, "ButtonNumber", 1 );
	priv->button_threshold = xf86SetIntOption( merged, "ButtonThreshold", 128 );

	s = xf86FindOptionValue (merged, "ReportingMode");
	if ((s) && (strcasecmp (s, "raw") == 0))
		priv->reporting_mode = TS_Raw;
	else
		priv->reporting_mode = TS_Scaled;

	priv->checksum = 0;

	/* 
	 * Create an X Input Serial Buffer if your device attaches to a serial
	 * port.
	 */
	priv->buffer = XisbNew (local->fd, 200);

	DBG (9, XisbTrace (priv->buffer, 1));

	/* 
	 * Verify that your hardware is attached and fuctional if you can
	 */
	if (QueryHardware (priv, errmaj, errmin) != Success)
	{
		ErrorF ("Unable to query/initialize SAMPLE hardware.\n");
		goto SetupProc_fail;
	}

	/* this results in an xstrdup that must be freed later */
	local->name = xf86SetStrOption( merged, "DeviceName", "SAMPLE XInput Device");

	/* Set the type that's appropriate for your device
	 * XI_KEYBOARD
	 * XI_MOUSE
	 * XI_TABLET
	 * XI_TOUCHSCREEN
	 * XI_TOUCHPAD
	 * XI_BARCODE
	 * XI_BUTTONBOX
	 * XI_KNOB_BOX
	 * XI_ONE_KNOB
	 * XI_NINE_KNOB
	 * XI_TRACKBALL
	 * XI_QUADRATURE
	 * XI_ID_MODULE
	 * XI_SPACEBALL
	 * XI_DATAGLOVE
	 * XI_EYETRACKER
	 * XI_CURSORKEYS
	 * XI_FOOTMOUSE
	 */
	local->type_name = XI_TOUCHSCREEN;
	/* 
	 * Standard setup for the local device record
	 */
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

	xf86AddLocalDevice( local, merged );

	/* return the LocalDevice */
	return (local);

	/* 
	 * If something went wrong, cleanup and return NULL
	 */
  SetupProc_fail:
	if ((local) && (local->fd))
		xf86CloseSerial (local->fd);
	if ((local) && (local->name))
		xfree (local->name);
	if (local)
		xfree (local);

	if ((priv) && (priv->buffer))
		XisbFree (priv->buffer);
	if (priv)
		xfree (priv);
	ErrorF ("SetupProc returning NULL\n");
	return (NULL);
}

/* 
 * The DeviceControl function should not need to be changed
 * except to remove ErrorFs
 */
static Bool
DeviceControl (DeviceIntPtr dev, int mode)
{
	Bool	RetValue;

	ErrorF ("DeviceControl called mode = %d\n", mode);
	switch (mode)
	{
	case DEVICE_INIT:
		ErrorF ("\tINIT\n");
		DeviceInit (dev);
		RetValue = Success;
		break;
	case DEVICE_ON:
		ErrorF ("\tON\n");
		RetValue = DeviceOn( dev );
		break;
	case DEVICE_OFF:
		ErrorF ("\tOFF\n");
		RetValue = DeviceOff( dev );
		break;
	case DEVICE_CLOSE:
		ErrorF ("\tCLOSE\n");
		RetValue = DeviceClose( dev );
		break;
	default:
		ErrorF ("\tBAD MODE\n");
		RetValue = BadValue;
	}

	return( RetValue );
}

/* 
 * The DeviceOn function should not need to be changed
 */
static Bool
DeviceOn (DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;

	AddEnabledDevice (local->fd);
	dev->public.on = TRUE;
	return (Success);
}

/* 
 * The DeviceOff function should not need to be changed
 */
static Bool
DeviceOff (DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;

	RemoveEnabledDevice (local->fd);
	dev->public.on = FALSE;
	return (Success);
}

/* 
 * The DeviceClose function should not need to be changed
 */
static Bool
DeviceClose (DeviceIntPtr dev)
{
	return (Success);
}

/* 
 * The DeviceInit function will need to be tailored to your device
 */
static Bool
DeviceInit (DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	SAMPLEPrivatePtr priv = (SAMPLEPrivatePtr) (local->private);
	Atom atom;
	unsigned char map[] =
	{0, 1};

	/* 
	 * Set up buttons, valuators etc for your device
	 */
	if (InitButtonClassDeviceStruct (dev, 1, map) == FALSE)
	{
		ErrorF ("Unable to allocate SAMPLE ButtonClassDeviceStruct\n");
		return !Success;
	}

	/* 
	 * this example device reports motions on 2 axes in absolute coordinates.
	 * Device may reports touch pressure on the 3rd axis.
	 */
	if (InitValuatorClassDeviceStruct (dev, 2, xf86GetMotionEvents,
									local->history_size, Absolute) == FALSE)
	{
		ErrorF ("Unable to allocate SAMPLE ValuatorClassDeviceStruct\n");
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
		ErrorF ("unable to allocate SAMPLE ProximityClassDeviceStruct\n");
		return !Success;
	}

	/* 
	 * Allocate the motion events buffer.
	 */
	xf86MotionHistoryAllocate (local);
	return (Success);
}

/* 
 * The ReadInput function will have to be tailored to your device
 */
static void
ReadInput (LocalDevicePtr local)
{
	int x, y, z;
	int state;
	SAMPLEPrivatePtr priv = (SAMPLEPrivatePtr) (local->private);

	/* 
	 * set blocking to -1 on the first call because we know there is data to
	 * read. Xisb automatically clears it after one successful read so that
	 * succeeding reads are preceeded buy a select with a 0 timeout to prevent
	 * read from blocking indefinately.
	 */
	XisbBlockDuration (priv->buffer, -1);
	while (SAMPLEGetPacket (priv) == Success)
	{
		/* 
		 * Examine priv->packet and call these functions as appropriate:
		 *
		 xf86PostProximityEvent
		 xf86PostMotionEvent
		 xf86PostButtonEvent
		 */
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
	*x = v0;
	*y = v1;
	return (Success);
}

/* 
 * the QueryHardware fuction should be tailored to your device to
 * verify the device is attached and functional and perform any
 * needed initialization.
 */
static Bool
QueryHardware (SAMPLEPrivatePtr priv, int *errmaj, int *errmin)
{
	return (Success);
}

/* 
 * This function should be renamed for your device and tailored to handle
 * your device's protocol.
 */
static Bool
SAMPLEGetPacket (SAMPLEPrivatePtr priv)
{
	int count = 0;
	int c;

	while ((c = XisbRead (priv->buffer)) >= 0)
	{
		/* 
		 * your checksum calculation may be different or your device's
		 * protocol may not have one.
		 */
		if (priv->lex_mode != SAMPLE_checksum)
			priv->checksum += c;
		/* 
		 * fail after 500 bytes so the server doesn't hang forever if a
		 * device sends bad data.
		 */
		if (count++ > 500)
			return (!Success);

		switch (priv->lex_mode)
		{
		case SAMPLE_normal:
			if (c == SAMPLE_SYNC_BYTE)
			{
				priv->packet[priv->packeti++] = (unsigned char) c;
				priv->checksum = SAMPLE_INIT_CHECKSUM + c;
				priv->lex_mode = SAMPLE_body;
			}
			break;

		case SAMPLE_body:
			if (priv->packeti < SAMPLE_BODY_LEN)
				priv->packet[priv->packeti++] = (unsigned char) c;
			if (priv->packeti == SAMPLE_BODY_LEN)
				priv->lex_mode = SAMPLE_checksum;
			break;

		case SAMPLE_checksum:

			if (c != priv->checksum)
			{
				xf86ErrorFVerb( 4, 
					   "Checksum mismatch. Read %d calculated %d\nPacket discarded.\n",
					   c, priv->checksum );
			}
			else
			{
				ErrorF ("got a good packet\n");
				return (Success);
			}
			break;
		}
	}
	return (!Success);
}
