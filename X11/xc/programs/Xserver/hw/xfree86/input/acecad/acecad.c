/* 
 * Copyright (c) 2001 Edouard TISSERANT <tissered@esstin.u-nancy.fr>
 * Parts inspired from Shane Watts <shane@bofh.asn.au> XFree86 3 Acecad Driver
 * Thanks to Emily, from AceCad, For giving me documents.
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
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/acecad/acecad.c,v 1.2 2001/11/26 16:25:52 dawes Exp $ */

#define _ACECAD_C_
/*****************************************************************************
 *	Standard Headers
 ****************************************************************************/

#ifdef LINUX_INPUT
#include <asm/types.h>
#include <linux/input.h>
#ifdef BUS_PCI
#undef BUS_PCI
#endif
#ifdef BUS_ISA
#undef BUS_ISA
#endif
#endif

#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#include <xf86_ansic.h>
#include <xf86_OSproc.h>
#include <xisb.h>
#include <xf86Xinput.h>
#include <exevents.h>
#include <xf86Module.h>

/*****************************************************************************
 *	Local Headers
 ****************************************************************************/
#include "acecad.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables
 ****************************************************************************/

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))
#undef read
#define read(a,b,c) xf86ReadSerial((a),(b),(c))

/* max number of input events to read in one read call */
#define MAX_EVENTS 50


static InputDriverRec ACECAD = 
{
	1,
	"acecad",
	NULL,
	AceCadPreInit,
	NULL,
	NULL,
	0
};


static XF86ModuleVersionInfo VersionRec =
{
	"acecad",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	1, 0, 0,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{0, 0, 0, 0}
};


static const char *default_options[] =
{
	"BaudRate", "9600",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "Odd",
	"Vmin", "1",
	"Vtime", "10",
	"FlowControl", "Xoff",
	NULL
};

XF86ModuleData acecadModuleData = { &VersionRec, SetupProc, TearDownProc};

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static pointer
SetupProc(	pointer module,
		pointer options,
		int *errmaj,
		int *errmin )
{
	xf86AddInputDriver(&ACECAD, module, 0);
	return module;
}

static void
TearDownProc( pointer p )
{
#if 0
	LocalDevicePtr local = (LocalDevicePtr) p;
	AceCadPrivatePtr priv = (AceCadPrivatePtr) local->private;

	DeviceOff (local->dev);

	xf86CloseSerial (local->fd);
	XisbFree (priv->buffer);
	xfree (priv);
	xfree (local->name);
	xfree (local);
#endif
}

static int
IsUSBLine(int fd)
{
    int version;
    int err;

    SYSCALL(err = ioctl(fd, EVIOCGVERSION, &version));
    
    if (!err) {
	xf86Msg(X_CONFIG,"Kernel Input driver version is %d.%d.%d\n",
	       version >> 16, (version >> 8) & 0xff, version & 0xff);
	return 1;
    } else {
	return 0;
    }
}


static InputInfoPtr
AceCadPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	LocalDevicePtr local = xf86AllocateInput(drv, 0);
	AceCadPrivatePtr priv = xcalloc (1, sizeof (AceCadPrivateRec));
	int speed;
	char *s;

	if ((!local) || (!priv))
		goto SetupProc_fail;

	memset(priv,0,sizeof (AceCadPrivateRec));

	local->name = dev->identifier;
	local->type_name = "AceCad Tablet";
	local->flags = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
	local->motion_history_proc = xf86GetMotionEvents;
	local->control_proc = NULL;
	local->close_proc = CloseProc;
	local->switch_mode = NULL;
	local->conversion_proc = ConvertProc;
	local->reverse_conversion_proc = ReverseConvertProc;
	local->dev = NULL;
	local->private = priv;
	local->private_flags = 0;
	local->conf_idev = dev;
	local->device_control = DeviceControl;
	/*local->always_core_feedback = 0;*/
	
	xf86CollectInputOptions(local, default_options, NULL);

	xf86OptionListReport(local->options);

	priv->acecadInc = xf86SetIntOption(local->options, "Increment", 0 );

	local->fd = xf86OpenSerial (local->options);
	if (local->fd == -1)
	{
		xf86Msg(X_ERROR,"AceCad driver unable to open device\n");
		goto SetupProc_fail;
	}
	xf86ErrorFVerb( 6, "tty port opened successfully\n" );

	if(IsUSBLine(local->fd)){
		priv->acecadUSB=1;

		local->read_input = USBReadInput;

		if (USBQueryHardware(local) != Success)
		{
			ErrorF ("Unable to query/initialize AceCad hardware.\n");
			goto SetupProc_fail;
		}
	}
	else{
		priv->acecadUSB=0;

		local->read_input = ReadInput;
		
		speed = xf86SetIntOption(local->options, "ReportSpeed", 85 );

		switch (speed)
		{
		case 120:
			priv->acecadReportSpeed = 'Q';
			break;
		case 85:
			priv->acecadReportSpeed = 'R';
			break;
		case 10:
			priv->acecadReportSpeed = 'S';
			break;
		case 2:
			priv->acecadReportSpeed = 'T';
			break;
		default:
			priv->acecadReportSpeed = 'R';
			speed = 85;
			xf86Msg(X_CONFIG, "Acecad Tablet: ReportSpeed possible values:\n 120, 85, 10, 2 \n");         
		}

		xf86Msg(X_CONFIG, "Acecad Tablet report %d points/s\n", speed);         

		priv->buffer = XisbNew (local->fd, 200);

		/* 
		 * Verify that hardware is attached and fuctional
		 */
		if (QueryHardware(priv) != Success)
		{
			xf86Msg(X_ERROR,"Unable to query/initialize AceCad hardware.\n");
			goto SetupProc_fail;
		}
	}

	s = xf86FindOptionValue(local->options, "Mode");
	if (s && (xf86NameCmp(s, "Relative") == 0))
	{
		priv->flags = priv->flags & ~ABSOLUTE_FLAG;
	}
	else 
	{
		priv->flags = priv->flags | ABSOLUTE_FLAG;
	}

	xf86Msg(X_CONFIG, "Acecad Tablet is in %s mode\n",(priv->flags & ABSOLUTE_FLAG) ? "absolute" : "relative");         
	DBG (9, XisbTrace (priv->buffer, 1));

	local->history_size = xf86SetIntOption(local->options , "HistorySize", 0);

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

	/* 
	 * If something went wrong, cleanup and return NULL
	 */
  SetupProc_fail:
	if ((local) && (local->fd))
		xf86CloseSerial (local->fd);
	if (local)
		xfree (local);

	if ((priv) && (priv->buffer))
		XisbFree (priv->buffer);
	if (priv)
		xfree (priv);
	return (NULL);
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
	char buffer[256];
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	AceCadPrivatePtr priv = (AceCadPrivatePtr) (local->private);

	xf86Msg(X_CONFIG, "Acecad Tablet Device On\n");

	local->fd = xf86OpenSerial(local->options);
	if (local->fd == -1)
	{
		xf86Msg(X_WARNING, "%s: cannot open input device\n", local->name);
		return (!Success);
	}

	
	if (priv->acecadUSB==0){
		priv->buffer = XisbNew(local->fd, 200);
		if (!priv->buffer) 
			{
				xf86CloseSerial(local->fd);
				local->fd = -1;
				return (!Success);
			}

		/*Rets qu'a l'envoyer a la tablette */
		sprintf(buffer, "%s%c%c%c%c", acecad_initstr, priv->acecadReportSpeed ,ACECAD_INCREMENT, 32 + priv->acecadInc, (priv->flags & ABSOLUTE_FLAG)? ACECAD_ABSOLUTE: ACECAD_RELATIVE);
		XisbWrite (priv->buffer, (unsigned char *)buffer, strlen(buffer));
	}
	
	xf86FlushInput(local->fd);
	xf86AddEnabledDevice (local);
	dev->public.on = TRUE;
	return (Success);
}

static Bool
DeviceOff (DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	AceCadPrivatePtr priv = (AceCadPrivatePtr) (local->private);


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


	xf86RemoveEnabledDevice (local);
	dev->public.on = FALSE;
	return (Success);
}

static Bool
DeviceClose (DeviceIntPtr dev)
{
	xf86Msg(X_CONFIG, "Acecad Tablet Device Close\n");
	return (Success);
}

static void 
ControlProc(DeviceIntPtr	device,
		   PtrCtrl	*ctrl)
{
	xf86Msg(X_CONFIG, "Acecad Tablet Control Proc\n");
}

static Bool
DeviceInit (DeviceIntPtr dev)
{
	int rx, ry;
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	AceCadPrivatePtr priv = (AceCadPrivatePtr) (local->private);
	unsigned char map[] =
	{0, 1, 2, 3};

	xf86Msg(X_CONFIG, "Acecad Tablet Device Init\n");

	/* 3 boutons */
	if (InitButtonClassDeviceStruct (dev, 3, map) == FALSE)
	{
		ErrorF ("Unable to allocate AceCad ButtonClassDeviceStruct\n");
		return !Success;
	}

	if (InitFocusClassDeviceStruct (dev) == FALSE)
	{
		ErrorF("Unable to allocate AceCad FocusClassDeviceStruct\n");
		return !Success;
	}

	if (InitPtrFeedbackClassDeviceStruct(dev,
	    				     ControlProc) == FALSE) {
		ErrorF("unable to init ptr feedback\n");
		return !Success;
	}


	/* 3 axes */
	if (InitValuatorClassDeviceStruct (dev, 3, xf86GetMotionEvents,
			local->history_size,
			((priv->flags & ABSOLUTE_FLAG)? Absolute: Relative)|OutOfProximity)
			 == FALSE)
	{
		ErrorF ("Unable to allocate AceCad ValuatorClassDeviceStruct\n");
		return !Success;
	}
	else
	{

		InitValuatorAxisStruct(dev,
			   0,
			   0,			/* min val */
			   priv->acecadMaxX,	/* max val */
			   1000,		/* resolution */
			   0,			/* min_res */
			   1000);		/* max_res */
		InitValuatorAxisStruct(dev,
			   1,
			   0,			/* min val */
			   priv->acecadMaxY,	/* max val */
			   1000,		/* resolution */
			   0,			/* min_res */
			   1000);		/* max_res */
		InitValuatorAxisStruct(dev,
			   2,
			   0,			/* min val */
			   priv->acecadMaxZ,	/* max val */
			   1000,		/* resolution */
			   0,			/* min_res */
			   1000);		/* max_res */

	}

        if (InitProximityClassDeviceStruct (dev) == FALSE)
        {
                ErrorF ("Unable to allocate ProximityClassDeviceStruct\n");
                return !Success;
        }

	xf86MotionHistoryAllocate (local);


	/* On ne peut pas calculer l'increment avant, faute d'ecran pour
	connaitre la taille... */

        if (priv->acecadInc > 95)
		priv->acecadInc = 95;
	if (priv->acecadInc < 1)
	{
		/* guess the best increment value given video mode */
		rx=priv->acecadMaxX / screenInfo.screens[0]->width;
		ry=priv->acecadMaxY / screenInfo.screens[0]->height;
		if (rx < ry)
			priv->acecadInc = rx;
		else
			priv->acecadInc = ry;
		if (priv->acecadInc < 1)
			priv->acecadInc = 1;
	}

	xf86Msg(X_CONFIG, "Acecad Tablet Increment: %d\n",priv->acecadInc);

	return (Success);
}

static void
ReadInput (LocalDevicePtr local)
{
	int x, y, z;
	int prox, buttons;
	int is_core_pointer, is_absolute;
	AceCadPrivatePtr priv = (AceCadPrivatePtr) (local->private);

	/*xf86Msg(X_CONFIG, "Acecad Tablet Read Input\n");*/

	is_absolute = (priv->flags & ABSOLUTE_FLAG);
	is_core_pointer = xf86IsCorePointer(local->dev);

	/* 
	 * set blocking to -1 on the first call because we know there is data to
	 * read. Xisb automatically clears it after one successful read so that
	 * succeeding reads are preceeded buy a select with a 0 timeout to prevent
	 * read from blocking indefinately.
	 */
	XisbBlockDuration (priv->buffer, -1);
	
	while (AceCadGetPacket (priv) == Success)
	{
		x = (int)priv->packet[1] | ((int)priv->packet[2] << 7);
		y = (int)priv->packet[3] | ((int)priv->packet[4] << 7);

	    if (!(priv->flags & ABSOLUTE_FLAG))
		{
			x = priv->packet[0] & XSIGN_BIT? x:-x;
			y = priv->packet[0] & YSIGN_BIT? y:-y;
		}
		else
		{
		y = priv->acecadMaxY - y ;
		}
		

		z = ((int)priv->packet[5] << 2) |
			(((int)priv->packet[6] & 0x01) << 1) |
			(((int)priv->packet[6] & 0x10) >> 4);

		buttons = ((int)priv->packet[0] & 0x07) |
			((int)priv->packet[6] & 0x02 << 2);

		prox = (priv->packet[0] & PROXIMITY_BIT)? 0: 1;

		if (prox)
		{
			if (!(priv->acecadOldProximity))
				if (!is_core_pointer)
				{
					/*xf86Msg(X_CONFIG, "Acecad Tablet ProxIN %d %d %d\n",x, y, z);*/
					xf86PostProximityEvent(local->dev, 1, 0, 3 , x, y, z);
				}

			if ((is_absolute && ((priv->acecadOldX != x) || (priv->acecadOldY != y) || (priv->acecadOldZ != z)))
				|| (!is_absolute && (x || y)))
			{
				if (is_absolute || priv->acecadOldProximity)
				{
					/*xf86Msg(X_CONFIG, "Acecad Tablet Motion %d %d %d\n", x, y, z);*/
					xf86PostMotionEvent(local->dev, is_absolute, 0, 3, x, y, z);
				}
			}

			if (priv->acecadOldButtons != buttons)
			{
				int	delta;

				delta = buttons ^ priv->acecadOldButtons;
				while(delta)
				{
					int id;

					id=ffs(delta);
					delta &= ~(1 << (id-1));

					/*xf86Msg(X_CONFIG, "Acecad Tablet Button %d 0x%x\n",id,(buttons&(1<<(id-1))));*/
					xf86PostButtonEvent(local->dev, is_absolute, id, (buttons&(1<<(id-1))), 0, 3, x, y,z);
				}
			}

			priv->acecadOldButtons = buttons;
			priv->acecadOldX = x;
			priv->acecadOldY = y;
			priv->acecadOldZ = z;
			priv->acecadOldProximity = prox;
		}
		else
		{
			if (!is_core_pointer)
				if (priv->acecadOldProximity)
				{
					/*xf86Msg(X_CONFIG, "Acecad Tablet ProxOUT %d %d %d\n",x, y, z);*/
					xf86PostProximityEvent(local->dev, 0, 0, 3, x,y,z);
				}
			priv->acecadOldProximity = 0;
		}
	}
	/*xf86Msg(X_CONFIG, "Acecad Tablet Sortie Read Input\n");*/
}

#define set_bit(byte,nb,bit)	(bit ? byte | (1<<nb) : byte & (~(1<<nb)))
static void
USBReadInput (LocalDevicePtr local)
{
	int len;
	struct input_event * event;
	char eventbuf[sizeof(struct input_event) * MAX_EVENTS];
	AceCadPrivatePtr priv = (AceCadPrivatePtr) (local->private);
	int x = priv->acecadOldX;
	int y = priv->acecadOldY;
	int z = priv->acecadOldZ;
	int prox = priv->acecadOldProximity;
	int buttons = priv->acecadOldButtons;
	int is_core_pointer;

	is_core_pointer = xf86IsCorePointer(local->dev);

	SYSCALL(len = read(local->fd, eventbuf, sizeof(eventbuf)));

	if (len <= 0) {
		ErrorF("Error reading wacom device : %s\n", strerror(errno));
		return;
	}

	for (event=(struct input_event *)eventbuf;
	 event<(struct input_event *)(eventbuf+len); event++) {
	 
		switch (event->type) {
		case EV_ABS:
		    switch (event->code) {
		    case ABS_X:
			x = event->value;
			break;

		    case ABS_Y:
			y = event->value;
			break;

		    case ABS_PRESSURE:
			z = event->value;
			break;

		    case ABS_MISC:
			break;

		    }
		    break; /* EV_ABS */

		case EV_KEY:
		    switch (event->code) {
		    case BTN_TOOL_PEN:
			prox = event->value;
			break;

		    case BTN_TOUCH:
			buttons=set_bit(buttons,0,event->value);
			break;

		    case BTN_STYLUS:
			buttons=set_bit(buttons,1,event->value);
			break;

		    case BTN_STYLUS2:
			buttons=set_bit(buttons,2,event->value);
			break;
		    }
		    break; /* EV_KEY */
		default:
		    xf86Msg(X_ERROR, "UNKNOWN event->code=%d\n", event->code);
		} /* switch event->type */

		/* ABS_MISC is the event terminator */
		if (event->type != EV_ABS || event->code != ABS_MISC) {
		    continue;
		}

		if (prox)
		{
			if (!(priv->acecadOldProximity))
				if (!is_core_pointer)
				{
					xf86PostProximityEvent(local->dev, 1, 0, 3 , x, y, z);
				}


			xf86PostMotionEvent(local->dev, 1, 0, 3, x, y, z);

			if (priv->acecadOldButtons != buttons)
			{
				int	delta;

				delta = buttons ^ priv->acecadOldButtons;
				while(delta)
				{
					int id;

					id=ffs(delta);
					delta &= ~(1 << (id-1));

					xf86PostButtonEvent(local->dev, 1, id, (buttons&(1<<(id-1))), 0, 3, x, y,z);
				}
			}
		}
		else
		{
			if (!is_core_pointer)
				if (priv->acecadOldProximity)
				{
					xf86PostProximityEvent(local->dev, 0, 0, 3, x,y,z);
				}
			priv->acecadOldProximity = 0;
		}

		priv->acecadOldButtons = buttons;
		priv->acecadOldX = x;
		priv->acecadOldY = y;
		priv->acecadOldZ = z;
		priv->acecadOldProximity = prox;
	}
	/*xf86Msg(X_CONFIG, "Acecad Tablet Sortie Read Input\n");*/
}

static void
CloseProc (LocalDevicePtr local)
{
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
    AceCadPrivatePtr	priv = (AceCadPrivatePtr)(local->private);

    *x = v0 * screenInfo.screens[0]->width / priv->acecadMaxX;
    *y = v1 * screenInfo.screens[0]->height / priv->acecadMaxY;
    return TRUE;
}


static Bool
ReverseConvertProc(		LocalDevicePtr	local,
			int		  x,
			int		  y,
			int		  *valuators)
{
    AceCadPrivatePtr	priv = (AceCadPrivatePtr)(local->private);

    valuators[0] = x * priv->acecadMaxX / screenInfo.screens[0]->width;
    valuators[1] = y * priv->acecadMaxY / screenInfo.screens[0]->height;

    return TRUE;
}


#define WriteString(str)\
XisbWrite (priv->buffer, (unsigned char *)(str), strlen(str))


static Bool
QueryHardware (AceCadPrivatePtr priv)
{	
	
	/* Reset */
	WriteString("z0");
	
	/* Wait */
	milisleep (250);
	
	/* Prompt Mode in order to not be disturbed */
	WriteString(ACECAD_PROMPT_MODE);
	
	/* Flush */
	while(XisbRead(priv->buffer)>=0);
	
	/* Ask for Config packet*/
	WriteString(ACECAD_CONFIG);
	
	/* Read the packet */
	XisbBlockDuration (priv->buffer, 1000000);
	NewPacket (priv);

	/*xf86Msg(X_CONFIG, "Acecad Tablet init envoyé \n");*/

	if ((AceCadGetPacket (priv) == Success))
	{
		priv->acecadMaxX = (int)priv->packet[1] + ((int)priv->packet[2] << 7);
		priv->acecadMaxY = (int)priv->packet[3] + ((int)priv->packet[4] << 7);
		priv->acecadMaxZ = 512;
		xf86Msg(X_CONFIG, "Acecad Tablet MaxX:%d MaxY:%d\n",priv->acecadMaxX,priv->acecadMaxY);
	}
	else
		return (!Success);
		
	/*xf86Msg(X_CONFIG, "Acecad Tablet query hardware fini \n");*/
	return (Success);
}

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)

static Bool
USBQueryHardware (LocalDevicePtr local)
{	
	AceCadPrivatePtr priv = (AceCadPrivatePtr) local->private;
	unsigned long	bit[EV_MAX][NBITS(KEY_MAX)];
	int			i, j;
	int			abs[5];
	char		name[256] = "Unknown";

	ioctl(local->fd, EVIOCGNAME(sizeof(name)), name);
	xf86Msg(X_CONFIG, "Kernel Input device name: \"%s\"\n", name);

	memset(bit, 0, sizeof(bit));
	ioctl(local->fd, EVIOCGBIT(0, EV_MAX), bit[0]);

    for (i = 0; i < EV_MAX; i++)
	if (test_bit(i, bit[0])) {
	    ioctl(local->fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
	    for (j = 0; j < KEY_MAX; j++) 
		if (test_bit(j, bit[i])) {
		    if (i == EV_ABS) {
			ioctl(local->fd, EVIOCGABS(j), abs);
			switch (j) {
			case ABS_X:
				priv->acecadMaxX = abs[2];
			break;
			    
			case ABS_Y:
				priv->acecadMaxY = abs[2];
			break;
			    
			case ABS_PRESSURE:
				priv->acecadMaxZ = abs[2];
			break;
			}
		    }
		}
	}
    
	xf86Msg(X_CONFIG, "Acecad Tablet MaxX:%d MaxY:%d MaxZ:%d\n",priv->acecadMaxX,priv->acecadMaxY,priv->acecadMaxZ);
	return (Success);
}

static void
NewPacket (AceCadPrivatePtr priv)
{
    priv->packeti = 0;
}

static Bool
AceCadGetPacket (AceCadPrivatePtr priv)
{
	int count = 0;
	int c = 0;

	while((c = XisbRead(priv->buffer))>=0 )
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

		if (c & PHASING_BIT)
		{
			NewPacket(priv);			
			
			/*xf86Msg(X_CONFIG, "Push %2.2x\n",(char) c);*/
			XisbBlockDuration (priv->buffer, 10000);
			priv->packet[priv->packeti++] = c;
			count=ACECAD_PACKET_SIZE-1;
			while(count-- && (c = XisbRead(priv->buffer))>=0)
				{
				/*xf86Msg(X_CONFIG, "Push %2.2x\n",(char) c);*/
				priv->packet[priv->packeti++] = c;
				}
			XisbBlockDuration (priv->buffer, 0);
			if(c > 0)
				return (Success);
		}
	}
	return (!Success);
}
