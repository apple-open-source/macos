/* 
 * Copyright (c) 2000,2001  Martin Kroeker (mk@daveg.com)
 * sample driver used :
 * Copyright (c) 1998  Metro Link Incorporated
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
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
 * Except as contained in this notice, the names of Martin Kroeker and/or
 * Daveg GmbH shall not be used in advertising or otherwise to promote the 
 * sale, use or other dealings in this Software without prior written 
 * authorization  from Martin Kroeker or Daveg GmbH.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/calcomp/xf86Calcomp.c,v 1.8 2003/01/15 03:43:58 dawes Exp $ */

#define _CALCOMP_C_
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
#include "xf86Calcomp.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/
#define DEBUG 1
/*****************************************************************************
 *	Local Variables
 ****************************************************************************/

InputDriverRec CALCOMP = {
                1,
                "calcomp",
                NULL,
                CalcompPreInit, /*preinit ?*/
                NULL,
                NULL,
                0
                };        

/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning
 */
static const char *default_options[] =
{
	"Device", "/dev/ttyS1",
	"BaudRate", "9600",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "None",
	"Vmin", "1",
	"Vtime", "1",
	"FlowControl", "None"
};
                                                        

#ifdef XFree86LOADER

static XF86ModuleVersionInfo VersionRec =
{
	"calcomp",
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
CalcompSetupProc(       pointer module,
                        pointer options,
                        int *errmaj,
                        int *errmin )
                        {
                        xf86LoaderReqSymLists(reqSymbols, NULL);
                        xf86AddInputDriver(&CALCOMP, module, 0);
                        return (pointer) 1;
			}                        

                                                                                                
                                                                                                
XF86ModuleData calcompModuleData = { &VersionRec, CalcompSetupProc, NULL };
                                                                                                
                                                                                                
#endif /* XFree86LOADER */
                                                                                                
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             
                                                        

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/


/* 
 * The TearDownProc may have to be tailored to your device
 */
/******************************************************
static void
TearDownProc( pointer p )
{
	LocalDevicePtr local = (LocalDevicePtr) p;
	CALCOMPPrivatePtr priv = (CALCOMPPrivatePtr) local->private;

	ErrorF ("Calcomp TearDownProc Called\n");

	DeviceOff (local->dev);


	xf86CloseSerial (local->fd);
	XisbFree (priv->buffer);
	xfree (priv);
	xfree (local->name);
	xfree (local);
}
**********************************************************/



/* 
 * The DeviceControl function should not need to be changed
 * except to remove ErrorFs
 */
static Bool
DeviceControl (DeviceIntPtr dev, int mode)
{
	Bool	RetValue;

/*	ErrorF ("DeviceControl called mode = %d\n", mode);*/
	switch (mode)
	{
	case DEVICE_INIT:
/*		ErrorF ("\tINIT\n");*/
		DeviceInit (dev);
		RetValue = Success;
		break;
	case DEVICE_ON:
/*		ErrorF ("\tON\n");*/
		RetValue = DeviceOn( dev );
		break;
	case DEVICE_OFF:
/*		ErrorF ("\tOFF\n");*/
		RetValue = DeviceOff( dev );
		break;
	case DEVICE_CLOSE:
/*		ErrorF ("\tCLOSE\n");*/
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
	CALCOMPPrivatePtr priv = (CALCOMPPrivatePtr) (local->private);
	unsigned char map[] =
	{0, 1};

	/* 
	 * Set up buttons, valuators etc for your device
	 */
	if (InitButtonClassDeviceStruct (dev, 1, map) == FALSE)
	{
		ErrorF ("Unable to allocate CALCOMP ButtonClassDeviceStruct\n");
		return !Success;
	}
	if (InitFocusClassDeviceStruct (dev) == FALSE) { /* is this necessary? */
		ErrorF ("Unable to allocate CALCOMP focus class device\n");
		return !Success;
	}	

	/* 
	 * this example device reports motions on 2 axes in absolute coordinates.
	 * Device may reports touch pressure on the 3rd axis.
	 */
	if (InitValuatorClassDeviceStruct (dev, 3, xf86GetMotionEvents,
									local->history_size, Absolute) == FALSE)
	{
		ErrorF ("Unable to allocate CALCOMP ValuatorClassDeviceStruct\n");
		return !Success;
	}
	else
	{
		InitValuatorAxisStruct (dev, 0, priv->min_x, priv->max_x,
								39400,
								0 /* min_res */ ,
								12000 /* max_res */ );
		InitValuatorAxisStruct (dev, 1, priv->min_y, priv->max_y,
								39400,
								0 /* min_res */ ,
								39400 /* max_res */ );

		InitValuatorAxisStruct (dev, 2, priv->min_z, priv->max_z,
								32,
								0 /* min_res */ ,
								32 /* max_res */ );
	}

	if (InitProximityClassDeviceStruct (dev) == FALSE)
	{
		ErrorF ("unable to allocate CALCOMP ProximityClassDeviceStruct\n");
		return !Success;
	}

         
        if (InitPtrFeedbackClassDeviceStruct(dev,
                         ControlProc) == FALSE) {
                ErrorF("unable to init ptr feedback\n");
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
	int x=0, y=0 , z=0;
	/*int state;*/
	int prox,buttons;
	Bool is_absolute = TRUE; /* FIXME */
	CALCOMPPrivatePtr priv = (CALCOMPPrivatePtr) (local->private);

	/* 
	 * set blocking to -1 on the first call because we know there is data to
	 * read. Xisb automatically clears it after one successful read so that
	 * succeeding reads are preceeded by a select with a 0 timeout to prevent
	 * read from blocking indefinitely.
	 */
	XisbBlockDuration (priv->buffer, -1);
	while (CALCOMPGetPacket (priv) == Success)
	{
		/* 
		 * Examine priv->packet and call these functions as appropriate:
		 *
		 xf86PostProximityEvent
		 xf86PostMotionEvent
		 xf86PostButtonEvent
		 */
		    /* Format of 6 bytes data packet in Calcomp Binary Encoding
		    
		           Byte 1
		                  bit 7  Phasing bit, always 1
		                  bit 6  Buttons on a 16bit cursor 
		                  bit 5  Button 3
		                  bit 4  Button 2
		                  bit 3  Button 1
		                  bit 2  Button 0 (Stylus tip)
		                  bit 1  X15
		                  bit 0  X14 
		           Byte 2 
		                  bit 7  Always 0
		                  bits 6-0 = X13 - X7
		           Byte 3 
		                  bit 7  Always 0
		                  bits 6-0 = X6 - X0
                           Byte 4
                                  bit 7  Always 0
                                  bit 6  Proximity
                                  bits 3-0 = Y17 - Y14
                           Byte 5 
                                  bit 7  Always 0
                                  bits 6-0 = Y13 - Y7
                           Byte 6 
                                  bit 7  Always 0
                                  bits 6-0 = Y6 - Y0
                    */                                                 
		    x = priv->packet[2] + priv->packet[1] * 128
		                         +(priv->packet[0] & 0x03) *128*128;
		    y = priv->packet[5] + priv->packet[4]*128 +priv->packet[3]*128*128;
		    y = priv->max_y -y;
		    prox = ((int)priv->packet[3] & PROXIMITY_BIT)? 0: 1;
		            
		    buttons = (((int)priv->packet[0] & BUTTON_BITS) >>2);
		    if (buttons && ! priv->pressure ) {
		    		if (buttons >15) {
		    		buttons = buttons -15;
				}else{
				buttons = buttons /2;
				if (buttons <4 ) buttons = buttons +1;
				}
			}else{
			z = buttons; /* button bits convey pressure data*/
			if (z >= priv->button_threshold) buttons = 1; 
#if 0
xf86Msg(X_INFO,"Tablett pressurebutton = %d >= %d\n",z,priv->button_threshold);
#endif
			}
#if 0
xf86Msg(X_INFO,"Tablett x y prox buttons = %d %d %d %d\n",x,y,prox,buttons);
#endif
		    if (prox) {
		               if (!(priv->prox)) {
		              xf86PostProximityEvent(local->dev, 1, 0, 2, x, y);
				}

                               if ((is_absolute && ((priv->x != x) || (priv->y != y)))
                                       || (!is_absolute && (x || y))) {
                    priv->x = x;
                    priv->y = y;

                                            if (is_absolute || priv->prox) {
                                                xf86PostMotionEvent(local->dev, is_absolute, 0, 3, x, y, z);

                                                }
                               } 
                    if (priv->buttons != buttons) {
                    int     delta;
                    int     button;
                    delta = buttons - priv->buttons;
                    button = (delta > 0)? delta: ((delta == 0)? priv->buttons : -delta);
                    if (priv->buttons != buttons) {
                    xf86PostButtonEvent(local->dev, is_absolute, button,
                                                   (delta > 0), 0, 2, x, y);
                                                        }
                    }
                    priv->buttons = buttons;
                    priv->x = x;
                    priv->y = y;
                    priv->prox = prox;
                 } else { /* !PROXIMITY */
                 /* Any changes in buttons are ignored when !proximity */
                   if (priv->prox) xf86PostProximityEvent(local->dev, 0, 0, 2, x, y);
                   priv->prox = 0;                                                                            
                 }                                                       
                                                                                                                       
	} /* while packets come in */
}

/* 
 * The ControlProc function may need to be tailored for your device
 */
static void
ControlProc (DeviceIntPtr device, PtrCtrl * control)
{
}
static int 
ChangeControlProc(LocalDevicePtr local, xDeviceCtl *control)
{
return(Success);
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
CALCOMPPrivatePtr priv=(CALCOMPPrivatePtr) local->private;
double factorX,factorY;

xf86Msg(X_INFO,"Calcomp ConvertProc called\n");
if (first !=0 || num == 1) return FALSE;

factorX= ((double) priv->screen_width)
	/(priv->max_x - priv->min_x);
factorY= ((double) priv->screen_height)
	/(priv->max_y - priv->min_y);
	
	*x = v0*factorX;
	*y = v1*factorY;
	return (Success);
}

/* 
 * the QueryHardware fuction should be tailored to your device to
 * verify the device is attached and functional and perform any
 * needed initialization.
 */
static Bool
QueryHardware (int fd, CALCOMPPrivatePtr priv)
{
char buffer[255];
int err;

 SYSCALL(err = write(fd, DB_COMM_SETUP, strlen(DB_COMM_SETUP)));
 	if (err<0) return !Success;
 	
 	xf86WaitForInput(-1,500);

 SYSCALL(err = write(fd, DB_UPPER_ORIGIN, strlen(DB_UPPER_ORIGIN)));
 	if (err<0) return !Success;
 	
 	xf86WaitForInput(-1,500);

 SYSCALL(err = write(fd, DB_BINARY_FMT, strlen(DB_BINARY_FMT)));
 	if (err<0) return !Success;

 	xf86WaitForInput(-1,500);

 SYSCALL(err = write(fd, DB_XINCREMENT, strlen(DB_XINCREMENT)));
 	if (err<0) return !Success;
 	
 	xf86WaitForInput(-1,500);

 SYSCALL(err = write(fd, DB_YINCREMENT, strlen(DB_YINCREMENT)));
 	if (err<0) return !Success;
 	
 	xf86WaitForInput(-1,500);

 SYSCALL(err = write(fd, DB_1000LPI, strlen(DB_1000LPI)));
 	if (err<0) return !Success;

 	xf86WaitForInput(-1,500);

 SYSCALL(err = write(fd, DB_STREAM_MODE, strlen(DB_STREAM_MODE)));
 	if (err<0) return !Success;
 	
 	xf86WaitForInput(-1,500);
	if (priv->pressure)
 	SYSCALL(err = write(fd, DB_PRESSURE_ON, strlen(DB_PRESSURE_ON)));
	else
 	SYSCALL(err = write(fd, DB_PRESSURE_OFF, strlen(DB_PRESSURE_OFF)));
	if (err<0) return !Success;
 	
 	xf86WaitForInput(-1,500);
 	
        if (!xf86CalWriteAndRead(fd, DB_FIRMID, buffer, 35, 1))
              return !Success;
              
        xf86Msg(X_INFO,"Calcomp firmware ID : %s\n", buffer);
	memset(buffer,32,35);

 	xf86WaitForInput(-1,500);

        if (!xf86CalWriteAndRead(fd, DB_PRODID, buffer, 20, 1))
              return !Success;
        xf86Msg(X_INFO,"Product ID : %s\n", buffer);

	memset(buffer,32,20);

 	xf86WaitForInput(-1,500);

        if (!xf86CalWriteAndRead(fd, DB_CONFIG, buffer, 6, 1))
              return !Success;
         priv->max_x = (int)buffer[2] + ((int)buffer[1] << 7) 
                                        + (((int)buffer[0]&0x03)<<14);
         priv->max_y = (int)buffer[5] + ((int)buffer[4] << 7);
                                                  
        xf86Msg(X_INFO,"Tablet size : %d x %d \n", priv->max_x,priv->max_y);


 	xf86WaitForInput(-1,500);
 	

 SYSCALL(err = write(fd, DB_ABSOLUTE, strlen(DB_ABSOLUTE)));
 	if (err<0) return !Success;
 	


/*
*/
	return (Success);
	
}

static InputInfoPtr
CalcompPreInit(       InputDriverPtr drv,
                        IDevPtr dev, int flags)
                        {

	InputInfoPtr local;
	CALCOMPPrivatePtr priv = xcalloc (1, sizeof (CALCOMPPrivateRec));
	char *s;

	    if (!(local = xf86AllocateInput(drv, 0)))
	            return NULL;
	            
	xf86Msg (X_INFO,"Calcomp SetupProc called\n");
	if ((!local) || (!priv))
		goto SetupProc_fail;

	local->conf_idev = dev;

        xf86CollectInputOptions(local, default_options, NULL);
        
                xf86OptionListReport( local->options );
                        local->fd = xf86OpenSerial (local->options);
                        






	if (local->fd == -1)
	{
		xf86Msg (X_ERROR,"CALCOMP driver unable to open device\n");
		goto SetupProc_fail;
	} else {
		xf86Msg( X_INFO,"CALCOMP driver: Serial device opened\n");
		}

	priv->min_x = xf86SetIntOption( local->options, "MinX", 0 );
	priv->max_x = xf86SetIntOption( local->options, "MaxX", 1000 );
	priv->min_y = xf86SetIntOption( local->options, "MinY", 0 );
	priv->max_y = xf86SetIntOption( local->options, "MaxY", 1000 );
	priv->min_z = xf86SetIntOption( local->options, "MinZ", 0 );
	priv->max_z = xf86SetIntOption( local->options, "MaxZ", 32 );
	priv->button_threshold = xf86SetIntOption (local->options, "ButtonThreshold", 16 );
	priv->pressure = xf86SetIntOption (local->options, "Pressure", 0);
	priv->untouch_delay = xf86SetIntOption( local->options, "UntouchDelay", 10 );
	priv->report_delay = xf86SetIntOption( local->options, "ReportDelay", 40 );
	priv->screen_num = xf86SetIntOption( local->options, "ScreenNumber", 0 );
	priv->button_number = xf86SetIntOption( local->options, "ButtonNumber", 1 );

	xf86Msg(X_INFO,"options read MaxX=%d, MaxY=%d\n",priv->max_x,priv->max_y);
	
	s = xf86FindOptionValue (local->options, "ReportingMode");
	if ((s) && (xf86NameCmp (s, "raw") == 0))
	        
		priv->reporting_mode = TS_Raw;
	else
		priv->reporting_mode = TS_Scaled;

	priv->checksum = 0;
	priv->buffer = XisbNew (local->fd, 200);

	DBG (9, XisbTrace (priv->buffer, 1));

	/* 
	 * Verify that your hardware is attached and fuctional if you can
	 */
	if (QueryHardware (local->fd, priv) != Success)
	{
		xf86Msg (X_ERROR,"Unable to query/initialize CALCOMP hardware.\n");
		goto SetupProc_fail;
	}else
		xf86Msg (X_INFO,"Calcomp tablet queried OK\n");
	
	local->name = xf86SetStrOption( local->options, "DeviceName", "CALCOMP XInput Device");
	xf86Msg(X_CONFIG," Calcomp device name  is  %s\n",local->name);	
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
	local->type_name = XI_TABLET;
	/* 
	 * Standard setup for the local device record
	 */
	local->device_control = DeviceControl;
	local->read_input = ReadInput;
	local->control_proc = ChangeControlProc;
	local->close_proc = CloseProc;
	local->switch_mode = SwitchMode;
	local->conversion_proc = ConvertProc;
	local->dev = NULL;
	local->private = priv;
	local->private_flags = 0;
	local->conf_idev = dev;
	local->history_size = xf86SetIntOption( local->options, "HistorySize", 0 );
	        local->flags |= XI86_CONFIGURED;
	                
                        xf86Msg(X_INFO,"Calcomp base setup finished\n");
                        return (local);
SetupProc_fail:
	xf86Msg (X_ERROR,"Calcomp setup failed, unloading tablet driver\n");
/* taken from xf86Wacom*/
	xfree (priv);
	xf86DeleteInput(local, 0);
/*         *          */	    
	return (NULL);


                        }
                                                                                                
/* 
 * This function should be renamed for your device and tailored to handle
 * your device's protocol.
 */
static Bool
CALCOMPGetPacket (CALCOMPPrivatePtr priv)
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

		
	if (c > 127) /* phasing bit set, start of packet */
			{
				priv->packeti=0;
				priv->packet[priv->packeti++] = (unsigned char) c;
			}
			else {	
			if (priv->packeti >0 && priv->packeti < CALCOMP_BODY_LEN)
				priv->packet[priv->packeti++] = (unsigned char) c;
			if (priv->packeti == CALCOMP_BODY_LEN)
			{
				priv->packeti=0;
				return (Success);
			}
		}
	}
	return (!Success);
}

/*
** xf86CalWriteAndRead
** Write data, and get the response.
*/
static char *
xf86CalWriteAndRead(int fd, char *data, char *buffer, int len, int cr_term)
{
    int err, numread = 0;
    int retries = 5;
    


		xf86FlushInput(fd);
		
    SYSCALL(err = write(fd, data, strlen(data)));
    if (err == -1) {
                   xf86Msg(X_ERROR,"Calcomp write");
                   return NULL;
                   }
do {
 err=xf86WaitForInput(fd, 2000);
      if (err < 0 ) {
                     xf86Msg(X_ERROR,"Calcomp select failed\n");
                     return NULL;
                     }
  retries--;
  }
  while (err<1 && retries >0);

if (retries<=0) {
       xf86Msg(X_WARNING,"Timeout while reading Calcomp tablet. No tablet connected ???\n");
	return NULL;
	}
	
    while (numread < len) {
      if (err == -1) {
                     xf86Msg(X_ERROR,"Calcomp select");
                     return NULL;
                     }
                     
    SYSCALL(err = read(fd, buffer + numread++, 1));
    if (err == -1) {
                   xf86Msg(X_ERROR,"Calcomp read");
                   return NULL;
                   }
    if (!err) {
              --numread;
              break;
              }
    if (cr_term && buffer[numread - 1] == '\r') {
                                          buffer[numread - 1] = 0;
                                          break;
                                          }
             }
                 buffer[numread] = 0;
                     return buffer;
                     }

