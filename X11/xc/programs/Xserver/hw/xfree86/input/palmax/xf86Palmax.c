/* $XFree86: xc/programs/Xserver/hw/xfree86/input/palmax/xf86Palmax.c,v 1.1 2002/11/22 03:40:22 dawes Exp $ */

#include "misc.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Xinput.h"
#include "exevents.h"
#include "os.h"

#ifdef XFree86LOADER
#include "xf86Module.h"
#endif

/*
 ***************************************************************************
 *
 * Default constants.
 *
 ***************************************************************************
 */

#define DEFAULT_MAX_X		63104
#define DEFAULT_MIN_X		8786
#define DEFAULT_MAX_Y		61592
#define DEFAULT_MIN_Y		7608
                              
#define XI_STYLUS		"TOUCHSCREEH"	/* X device name for the stylus device	*/


/*
 ***************************************************************************
 *
 * Protocol constants.
 *
 ***************************************************************************
 */

#define PMX_REPORT_SIZE		3	/* Size of a report packet.			*/
#define PMX_BUFFER_SIZE		256	/* Size of input buffer.			*/
#define PMX_PACKET_SIZE		5	/* Maximum size of a command/reply *including*	*/

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

					/* the leading and trailing bytes.		*/

/*
 ***************************************************************************
 *
 * Device private records.
 *
 ***************************************************************************
 */

typedef struct _PMXPrivateRec {
  char			*input_dev;	/* The touchscreen input tty			*/
  OsTimerPtr		uptimer;	/* Timeout on stream				*/
  OsTimerPtr		polltimer;	/* Button poll timer				*/
  int			min_x;		/* Minimum x reported by calibration		*/
  int			max_x;		/* Maximum x					*/
  int			min_y;		/* Minimum y reported by calibration		*/
  int			max_y;		/* Maximum y					*/
  int			cur_x;		/* Current x					*/
  int			cur_y;		/* Current y					*/
  int			button1;	/* True if finger button 1 is down		*/
  int 			button2;	/* True if finger button 2 is down		*/
  int			button3;	/* 1 if button3 down, 2 on wait for up		*/
  int			screen_no;	/* Screen associated with the device		*/
  int			screen_width;	/* Width of the associated X screen		*/
  int			screen_height;	/* Height of the screen				*/
  Bool			inited;		/* The controller has already been configured ?	*/
  char			state;		/* Current state of report flags.		*/
  int			num_old_bytes;	/* Number of bytes left in receive buffer.	*/
  LocalDevicePtr	stylus;		/* Stylus device ptr associated with the hw.	*/
  int			swap_axes;	/* Swap X an Y axes if != 0 */
  int			tap_button;	/* Tapping is button 0 */
  unsigned char		rec_buf[PMX_BUFFER_SIZE]; /* Receive buffer.			*/
} PMXPrivateRec, *PMXPrivatePtr;


/*
 ***************************************************************************
 *
 * xf86PmxConvert --
 *	Convert extended valuators to x and y suitable for core motion
 *	events. Return True if ok and False if the requested conversion
 *	can't be done for the specified valuators.
 *
 ***************************************************************************
 */
static Bool xf86PmxConvert(LocalDevicePtr local, int first, int num,
		int v0,int v1,int v2,int v3, int v4, int v5, int *x, int *y)
{
	PMXPrivatePtr	priv = (PMXPrivatePtr) local->private;
	int width = priv->max_x - priv->min_x;
	int height = priv->max_y - priv->min_y;
	int input_x, input_y;
	int tmp;

	input_x = v0;
	input_y = v1;
  
	if (priv->swap_axes) {
		tmp = input_x;
		input_x = input_y;
		input_y = tmp;
	}
	*x = (priv->screen_width * (input_x - priv->min_x)) / width;
	*y = (priv->screen_height * (input_y - priv->min_y)) / height;
	/*
	 * Need to check if still on the correct screen.
	 * This call is here so that this work can be done after
	 * calib and before posting the event.
	 */
	xf86XInputSetScreen(local, priv->screen_no, *x, *y);
	return TRUE;
}


/*
 *	Unpack a two byte code from the Palmax touchscreen 
 */

static int unpack(unsigned int v1, unsigned int v2)
{
	/* Actually v1 lower 6 bits are not used */
	return (v2 << 8) | (v1);
}

static CARD32 PalmaxUpTimeout(OsTimerPtr timer, CARD32 now, pointer arg)
{
	PMXPrivatePtr priv = arg;
	if(priv->state)
	{
		int sigstate = xf86BlockSIGIO ();
		/* Post a button up event */
		xf86PostButtonEvent(priv->stylus->dev, TRUE, 1, 0,
			0, 2, priv->cur_x >> 4, priv->cur_y >> 4);
		priv->state = 0;
		xf86UnblockSIGIO (sigstate);
	}
	return 0;
}

/*
 *	Poll the Palmax finger buttons ten times a second
 */

static CARD32 PalmaxPollTimeout(OsTimerPtr timer, CARD32 now, pointer arg)
{
	LocalDevicePtr local = (LocalDevicePtr) arg;
	PMXPrivatePtr priv = (PMXPrivatePtr)(local->private);
	int modembits;
	int button1 = 0, button2 = 0;
	int sigstate = xf86BlockSIGIO ();

	modembits = xf86GetSerialModemState(local->fd);	
	
	if (modembits & XF86_M_CTS) 
		button1 = 1;
		
	if (modembits & XF86_M_DSR)
		button2 = 1;
		
	/*
	 *	Check for chording for "middle" button
	 */
	if(button1 == 1 && button2 == 1 && priv->button1 == 0 && priv->button2 == 0)
	{
		if(!priv->button3)
		{
			/* Middle button emulation - down */
			xf86PostButtonEvent(priv->stylus->dev, TRUE, 3, 1,
				0, 2, priv->cur_x >> 4, priv->cur_y >> 4);
			priv->button3 = 1;
		}
	}
	
	/*
	 *	Check if we are chording and haven't yet released
	 *	both buttons
	 */
	if(priv->button3 && (button1 == 0 || button2 == 0))
	{
		if(priv->button3 != 2)
		{
			/* Middle button emulation - up */
			xf86PostButtonEvent(priv->stylus->dev, TRUE, 3, 0,
				0, 2, priv->cur_x >> 4, priv->cur_y >> 4);
			priv->button3 = 2;
		}
		/*
		 *	Wait for both buttons to go up
		 */
		if(button1 || button2)
			goto out;
		priv->button3 = 0;
	}
	if(button1 != priv->button1)
	{		
		xf86PostButtonEvent(priv->stylus->dev, TRUE, 1, button1,
			0, 2, priv->cur_x >> 4, priv->cur_y >> 4);
		priv->button1 = button1;
	}
	if(button2 != priv->button2)
	{		
		xf86PostButtonEvent(priv->stylus->dev, TRUE, 2, button2,
			0, 2, priv->cur_x >> 4, priv->cur_y >> 4);
		priv->button2 = button2;
	}

out:	
	xf86UnblockSIGIO (sigstate);
	return 100;
}
	
/*
 ***************************************************************************
 *
 * xf86PmxReadInput --
 *	Read a buffer full of input from the touchscreen and enqueue
 *	all report packets found in it.
 *	The routine can work with any of the two X device structs associated
 *	with the touchscreen. It is always possible to find the relevant
 *	informations and to emit the events for both devices if provided
 *	with one of the two structs. This point is relevant only if the
 *	two devices are actives at the same time.
 *

 ***************************************************************************
 */
static void xf86PmxReadInput(LocalDevicePtr local)
{
	PMXPrivatePtr		priv = (PMXPrivatePtr)(local->private);
	int cur_x, cur_y;
	int state;
	int num_bytes;
	int bytes_in_packet;
	unsigned char *ptr, *start_ptr;
	int report_size = 3;
  
	/*
	 * Try to get a buffer full of report packets.
	 */

	num_bytes = xf86ReadSerial(local->fd, (char *) (priv->rec_buf + priv->num_old_bytes),
			   PMX_BUFFER_SIZE - priv->num_old_bytes);
	if (num_bytes < 0) {
		Error("System error while reading from Palmax touchscreen.");
		return;
	}

	num_bytes += priv->num_old_bytes;
	ptr = priv->rec_buf;
	bytes_in_packet = 0;
	start_ptr = ptr;

	while (num_bytes >= report_size) 
	{
		/*
		 * Skip bytes until we hit a header (FE/FF)
		 */
		 
		switch(bytes_in_packet)
		{
			case 0:
				if(ptr[0] != 0xFF)
					start_ptr++;
				else
					bytes_in_packet++;
				break;
			case 1:
				if(ptr[0] == 0xFE)
					report_size = 3;
				else
					report_size = 5;
				bytes_in_packet++;
				break;
			default:
				bytes_in_packet++;
				break;
		}
		num_bytes--;
		ptr++;

		if (bytes_in_packet == report_size) 
		{
			/*
			 * First stick together the various pieces.
			 */

			if(priv->uptimer)
			{
				TimerFree(priv->uptimer);
				priv->uptimer = NULL;
			}
			
			state = 1;
			if(start_ptr[1] == 0xFE)
				state = 0;
			else
			{
				int new_x = unpack(start_ptr[1], start_ptr[2]);
				int new_y = unpack(start_ptr[3], start_ptr[4]);
				int shift = abs(new_x - (priv->cur_x >> 4));
				shift += abs(new_y - (priv->cur_y >> 4));
				
				if(shift < 1400)
				{
					/* We work to a base of 16 times the pointer, and do smoothing */
					cur_y = ((priv->cur_y * 15) >> 4) + new_y;
					cur_x = ((priv->cur_x * 15) >> 4) + new_x;
				}
				else if(shift < 3000)
				{
					/* We work to a base of 16 times the pointer, and do smoothing */
					cur_y = ((priv->cur_y * 7) >> 3) + (new_y << 1);
					cur_x = ((priv->cur_x * 7) >> 3) + (new_x << 1);
				}
				else if(shift < 6000)
				{
					/* We work to a base of 16 times the pointer, and do smoothing */
					cur_y = ((priv->cur_y * 3) >> 2) + (new_y << 2);
					cur_x = ((priv->cur_x * 3) >> 2) + (new_x << 2);
					/* Supress button change until the jitter filter has kicked in */
					state = priv->state;
				}
				else
				{
					cur_y = new_y << 4;
					cur_x = new_x << 4;
					/* Supress button change until the jitter filter has kicked in */
					state = priv->state;
				}
				xf86PostMotionEvent(priv->stylus->dev, TRUE, 0, 2, cur_x >> 4, cur_y >> 4);
				priv->cur_x = cur_x;
				priv->cur_y = cur_y;
			}
	      
			start_ptr = ptr;
			bytes_in_packet = 0;
      
			/*
			 * Emit a button press or release.
			 */
			if (state != priv->state && priv->tap_button) 
			{
				/* Post the event, or in the first button since both may 
				   impact the value */
				xf86PostButtonEvent(priv->stylus->dev, TRUE, 1, state|priv->button1,
					0, 2, priv->cur_x >> 4, priv->cur_y >> 4);
				priv->state = state;
				if(state == 1)
					priv->uptimer = TimerSet(priv->uptimer, 0, 100, PalmaxUpTimeout, priv);
			}
		}
	}

	/*
	 * If some bytes are left in the buffer, pack them at the
	 * beginning for the next turn.
	 */
	if (num_bytes != 0) {
		memcpy(priv->rec_buf, ptr, num_bytes);
		priv->num_old_bytes = num_bytes;
	}
	else
	{
		priv->num_old_bytes = 0;
	}
}


static void PMXPtrCtrl(DeviceIntPtr device, PtrCtrl *ctrl)
{
	/* I have no clue what this does, except that registering it stops the 
	   X server segfaulting in ProcGetPointerMapping()
	   Ho Hum.
	*/
}

/*
 ***************************************************************************
 *
 * xf86PmxControl --
 *
 ***************************************************************************
 */
static Bool
xf86PmxControl(DeviceIntPtr	dev,
	       int		mode)
{
	static unsigned char map[] = { 0, 1, 3, 2};
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	PMXPrivatePtr priv = (PMXPrivatePtr)(local->private);
	
	switch(mode)
	{

	case DEVICE_INIT:
		if (priv->screen_no >= screenInfo.numScreens || priv->screen_no < 0) {
			priv->screen_no = 0;
		}
		priv->screen_width = screenInfo.screens[priv->screen_no]->width;
		priv->screen_height = screenInfo.screens[priv->screen_no]->height;

		/*
		 * Device reports button press for up to 3 buttons.
		 */
		if (InitButtonClassDeviceStruct(dev, 3, map) == FALSE) {
			ErrorF("Unable to allocate ButtonClassDeviceStruct\n");
			return !Success;
		}
      	
		/*
		 * Device reports motions on 2 axes in absolute coordinates.
		 * Axes min and max values are reported in raw coordinates.
		 * Resolution is computed roughly by the difference between
		 * max and min values scaled from the approximate size of the
		 * screen to fit one meter.
		 */
	
		if (InitValuatorClassDeviceStruct(dev, 2, xf86GetMotionEvents,
				local->history_size, Absolute) == FALSE) {
			ErrorF("Unable to allocate ValuatorClassDeviceStruct\n");
			return !Success;
		}
		else
		{
			InitValuatorAxisStruct(dev, 0, priv->min_x, priv->max_x,
				       65535,
				       0     /* min_res */,
				       65535  /* max_res */);
			InitValuatorAxisStruct(dev, 1, priv->min_y, priv->max_y,
				       65535,	/* resolution */
				       0     /* min_res */,
				       65535 /* max_res */);
		}
	      
		if (InitFocusClassDeviceStruct(dev) == FALSE) {
			ErrorF("Unable to allocate FocusClassDeviceStruct\n");
		}
		
		if(InitPtrFeedbackClassDeviceStruct(dev, PMXPtrCtrl) == FALSE) {
			ErrorF("Unable to allocate PtrFeedBackClassDeviceStruct\n");
		}
	      
		/*
		 * Allocate the motion events buffer.
		 */

		xf86MotionHistoryAllocate(local);
      
		return Success;
    
	case DEVICE_ON:
		if (local->fd < 0) {
			local->fd = xf86OpenSerial(local->options);
			if (local->fd < 0) {
				Error("Unable to open Palmax touchscreen device");
				return !Success;
			}
			AddEnabledDevice(local->fd);
		}
		priv->polltimer = TimerSet(priv->polltimer, 0, 100, PalmaxPollTimeout, local);

		dev->public.on = TRUE; 
		return Success;
      
	case DEVICE_OFF:
		if(priv->polltimer)
		{
			TimerFree(priv->polltimer);
			priv->polltimer = NULL;
		}
		dev->public.on = FALSE;
		return Success;
    
	case DEVICE_CLOSE:
		dev->public.on = FALSE;
		if(priv->uptimer)
		{
			TimerFree(priv->uptimer);
			priv->uptimer = NULL;
		}
		if (local->fd >= 0) {
			xf86RemoveEnabledDevice(local);
			xf86CloseSerial(local->fd);
			local->fd = -1;
		}
		return Success;
	default:
		ErrorF("unsupported mode=%d\n", mode);
		return !Success;
	}
}

/*
 ***************************************************************************
 *
 * xf86PmxControlProc --
 *
 ***************************************************************************
 */
 
static int xf86PmxControlProc (InputInfoPtr pInfo, xDeviceCtl * control)
{
#if 0
	xDeviceTSCalibrationCtl *c = (xDeviceTSCalibrationCtl *) control;
	PMXPrivatePtr priv = (PMXPrivatePtr) (pInfo->private);
        priv->min_x = c->min_x;
        priv->max_x = c->max_x;
        priv->min_y = c->min_y;
        priv->max_y = c->max_y;
#endif 
	return (Success);
}

/*
 ***************************************************************************
 *
 * xf86PmxAllocate --
 *
 ***************************************************************************
 */
static LocalDevicePtr xf86PmxAllocate(InputDriverPtr drv, char *name, char *type_name, int flag)
{
	LocalDevicePtr local = xf86AllocateInput(drv, 0);
	PMXPrivatePtr priv = (PMXPrivatePtr) xalloc(sizeof(PMXPrivateRec));
  
	if (!local || !priv)
	{
		if(priv)
			xfree(priv);
		if(local)
			xfree(local);
		return NULL;
	}

	priv->input_dev = strdup("/dev/ttyS0");
	priv->min_x = 0;
	priv->max_x = 0;
	priv->min_y = 0;
	priv->max_y = 0;
	priv->screen_no = 0;
	priv->screen_width = -1;
	priv->screen_height = -1;
	priv->inited = 0;
	priv->state = 0;
	priv->num_old_bytes = 0;
	priv->swap_axes = 0;
	priv->tap_button = 0;
	priv->uptimer = NULL;
	priv->polltimer = NULL;
	priv->button1 = 0;
	priv->button2 = 0;
  
	local->name = name;
	local->flags = 0 /* XI86_NO_OPEN_ON_INIT */;
	local->device_control = xf86PmxControl;
	local->read_input = xf86PmxReadInput;
	local->control_proc = xf86PmxControlProc;
	local->close_proc = NULL;
	local->switch_mode = NULL;
	local->conversion_proc = xf86PmxConvert;
	local->reverse_conversion_proc = NULL;
	local->fd = -1;
	local->atom = 0;
	local->dev = NULL;
	local->private = priv;
	local->private_flags = flag;
	local->type_name = type_name;
	local->history_size = 0;
  
	return local;
}


/*
 ***************************************************************************
 *
 * xf86PmxAllocateStylus --
 *
 ***************************************************************************
 */
static LocalDevicePtr xf86PmxAllocateStylus(InputDriverPtr drv)
{
	LocalDevicePtr local = xf86PmxAllocate(drv, XI_STYLUS, "Palmax Stylus", 1);
	if (local)
		((PMXPrivatePtr) local->private)->stylus = local;
	return local;
}


static void xf86PmxUninit(InputDriverPtr drv, LocalDevicePtr local, int flags)
{
	PMXPrivatePtr priv = (PMXPrivatePtr) local->private;
  
	xf86PmxControl(local->dev, DEVICE_OFF);

	if (priv) {
		priv->stylus->private = NULL;
		xfree(priv->input_dev);
		xfree(priv);
	}
	xfree(local->name);
	xfree(local);

	xf86DeleteInput(local, 0);
}

static const char *default_options[] = {
	"BaudRate", "19200",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "None",
	"Vmin", "1",
	"Vtime", "1",
	"FlowControl", "None",
	NULL
};

static InputInfoPtr xf86PmxInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	LocalDevicePtr local=NULL, fake_local=NULL;
	PMXPrivatePtr priv=NULL;
	char *str;
	int portrait=0;

	fake_local = (LocalDevicePtr) xcalloc(1, sizeof(LocalDeviceRec));
	if (!fake_local) {
		goto init_err;
	}
	fake_local->conf_idev = dev;
  
	xf86CollectInputOptions(fake_local, default_options, NULL);

	local = xf86PmxAllocateStylus(drv);
	priv = local->private;
	local->options = fake_local->options;
	local->conf_idev = fake_local->conf_idev;
	xfree(fake_local);
	fake_local = NULL;
  
	str = xf86FindOptionValue(local->options, "Device");
	if (!str) {
		xf86Msg(X_ERROR, "%s: No Device specified in Palmax module config.\n", dev->identifier);
		goto init_err;
	}
	priv->input_dev = strdup(str);
	priv->stylus = local;

	/* Process the common options. */
	xf86ProcessCommonOptions(local, local->options);
  
	str = xf86FindOptionValue(local->options, "DeviceName");
	if (str) {
		local->name = strdup(str);
	}
	xf86Msg(X_CONFIG, "Palmax X device name: %s\n", local->name);  
	priv->screen_no = xf86SetIntOption(local->options, "ScreenNo", 0);
	xf86Msg(X_CONFIG, "Palmax associated screen: %d\n", priv->screen_no);  
	priv->max_x = xf86SetIntOption(local->options, "MaxX", DEFAULT_MAX_X);
	xf86Msg(X_CONFIG, "Palmax maximum x position: %d\n", priv->max_x);
	priv->min_x = xf86SetIntOption(local->options, "MinX", DEFAULT_MIN_X);
	xf86Msg(X_CONFIG, "Palmax minimum x position: %d\n", priv->min_x);
	priv->max_y = xf86SetIntOption(local->options, "MaxY", DEFAULT_MAX_Y);
	xf86Msg(X_CONFIG, "Palmax maximum y position: %d\n", priv->max_y);
	priv->min_y = xf86SetIntOption(local->options, "MinY", DEFAULT_MIN_Y);
	xf86Msg(X_CONFIG, "Palmax minimum y position: %d\n", priv->min_y);
	priv->tap_button = xf86SetBoolOption(local->options, "TapButton", 0);
	if(priv->tap_button)
		xf86Msg(X_CONFIG, "Palmax touchpad acts as button\n");
	priv->swap_axes = xf86SetBoolOption(local->options, "SwapXY", 0);
	if (priv->swap_axes) {
		xf86Msg(X_CONFIG, "Palmax %s device will work with X and Y axes swapped\n",
			local->name);
	}
	str = xf86SetStrOption(local->options, "PortraitMode", "Landscape");
	if (strcmp(str, "Portrait") == 0) {
		portrait = 1;
	}
	else if (strcmp(str, "PortraitCCW") == 0) {
		portrait = -1;
	}
	else if (strcmp(str, "Landscape") != 0) {
		xf86Msg(X_ERROR, "Palmax portrait mode should be: Portrait, Landscape or PortraitCCW");
		str = "Landscape";
	}
	xf86Msg(X_CONFIG, "Palmax device will work in %s mode\n", str);      
  
	if (priv->max_x - priv->min_x <= 0) {
		xf86Msg(X_INFO, "Palmax: reverse x mode (minimum x position >= maximum x position)\n");
	}  
	if (priv->max_y - priv->min_y <= 0) {
		xf86Msg(X_INFO, "Palmax: reverse y mode (minimum y position >= maximum y position)\n");    
	}

	if (portrait == 1) {
		/*
		 * Portrait Clockwise: reverse Y axis and exchange X and Y.
		 */
		int tmp;
		tmp = priv->min_y;
		priv->min_y = priv->max_y;
		priv->max_y = tmp;
		priv->swap_axes = (priv->swap_axes==0) ? 1 : 0;
	}
	else if (portrait == -1) {
		/*
		 * Portrait Counter Clockwise: reverse X axis and exchange X and Y.
		 */
		int tmp;
		tmp = priv->min_x;
		priv->min_x = priv->max_x;
		priv->max_x = tmp;
		priv->swap_axes = (priv->swap_axes==0) ? 1 : 0;
	}

	/* mark the device configured */
	local->flags |= XI86_CONFIGURED;
	return local;

init_err:
	if (fake_local)
		xfree(fake_local);
	if (priv) {
		if (priv->input_dev)
			xfree(priv->input_dev);
		xfree(priv);
	}
	if (local)
		xfree(local);
	return NULL;
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec PALMAX = {
	1,			/* driver version */
	"palmax",		/* driver name */
	NULL,			/* identify */
	xf86PmxInit,		/* pre-init */
	xf86PmxUninit,		/* un-init */
	NULL,			/* module */
	0			/* ref count */
};

#ifdef XFree86LOADER
static pointer Plug(pointer module, pointer options, int *errmaj,int *errmin)
{
	xf86LoaderReqSymLists(reqSymbols, NULL);
	xf86AddInputDriver(&PALMAX, module, 0);
	return module;
}

static void Unplug(pointer p)
{
}

static XF86ModuleVersionInfo version_rec = {
	"palmax",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	1, 0, 0,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{ 0, 0, 0, 0 }
};

/*
 * This is the entry point in the module. The name
 * is setup after the pattern <module_name>ModuleData.
 * Do not change it.
 */
XF86ModuleData palmaxModuleData = { &version_rec, Plug, Unplug };
#endif

