/* $XFree86: xc/programs/Xserver/hw/xfree86/input/ur98/xf86Ur-98.c,v 1.2 2003/12/22 17:48:10 tsi Exp $ */

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#define inline __inline__
#include <linux/joystick.h>

#undef BUS_ISA
#undef BUS_PCI

#include <fcntl.h>
#include <sys/ioctl.h>
#include "misc.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Xinput.h"
#include "exevents.h"
#include "os.h"

#ifdef XFree86LOADER
#include "xf86Module.h"

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
#endif



/*
 ***************************************************************************
 *
 * Default constants.
 *
 ***************************************************************************
 */

#define DEFAULT_MAX_X		32768
#define DEFAULT_MIN_X		0
#define DEFAULT_MAX_Y		65535
#define DEFAULT_MIN_Y		0
#define DEFAULT_MAX_Z		65535
#define DEFAULT_MIN_Z		0
#define DEFAULT_MAX_T		65535
#define DEFAULT_MIN_T		0

#define DEFAULT_HEAD_THRESH	38000
#define DEFAULT_HEAD_LOCK	450
                              
#define XI_STYLUS		"HEAD"	/* X device name for the head tracking device	*/


/*
 ***************************************************************************
 *
 * Device private records.
 *
 ***************************************************************************
 */

typedef struct _UR98PrivateRec {
  char			*input_dev;	/* The head screen input device			*/
  int			min_x;		/* Minimum x reported by calibration		*/
  int			max_x;		/* Maximum x					*/
  int			min_y;		/* Minimum y reported by calibration		*/
  int			max_y;		/* Maximum y					*/
  int			min_z;		/* Minimum z					*/
  int			max_z;		/* Maximum z					*/
  int			min_t;		/* Minimum t					*/
  int			max_t;		/* Maximum t					*/
  int			cur_x;		/* Current x					*/
  int			cur_y;		/* Current y					*/
  int			cur_z;		/* Current z					*/
  int			cur_t;		/* Current t					*/
  int			axes;		/* Number of axes				*/
  int			button_5;	/* True if axis 4 is remapped as button5 	*/
  int			buttons[5];	/* Button status				*/
  int			screen_no;	/* Screen associated with the device		*/
  int			screen_width;	/* Width of the associated X screen		*/
  int			screen_height;	/* Height of the screen				*/
  Bool			inited;		/* The controller has already been configured ?	*/
  char			state;		/* Current state of report flags.		*/
  LocalDevicePtr	head;		/* Device ptr associated with the hw.		*/
  int			swap_axes;	/* Swap X an Y axes if != 0 			*/
  int			head_button;	/* Z is button 0, cur_z is holds true cur_t	*/
  int			head_lock;	/* Lock for x/y in head_button mode		*/
  int			head_thresh;	/* Threshhold for Z as button down		*/
} UR98PrivateRec, *UR98PrivatePtr;


/*
 ***************************************************************************
 *
 * xf86UR98Convert --
 *	Convert extended valuators to x and y suitable for core motion
 *	events. Return True if ok and False if the requested conversion
 *	can't be done for the specified valuators.
 *
 ***************************************************************************
 */
static Bool xf86UR98Convert(LocalDevicePtr local, int first, int num,
		int v0,int v1,int v2,int v3, int v4, int v5, int *x, int *y)
{
	UR98PrivatePtr	priv = (UR98PrivatePtr) local->private;
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

static void UR98PtrCtrl(DeviceIntPtr device, PtrCtrl *ctrl)
{
	/* I have no clue what this does, except that registering it stops the 
	   X server segfaulting in ProcGetPointerMapping()
	   Ho Hum.
	*/
}

static void xf86UR98ReadInput(LocalDevicePtr local)
{
	UR98PrivatePtr		priv = (UR98PrivatePtr)(local->private);
	struct js_event event;
	int one=1;
	
	ioctl(local->fd, FIONBIO, &one);
		
	while(read(local->fd, &event, sizeof(event))==sizeof(event))
	{
		if(event.type & JS_EVENT_BUTTON)
		{
			/* Shift the buttons if Z is button 1 */
			event.number += priv->head_button;
			if(priv->buttons[event.number] != event.value)
			{
				priv->buttons[event.number] = event.value;
				xf86PostButtonEvent(priv->head->dev, TRUE, event.number + 1, event.value,
						0, priv->axes, priv->cur_x, priv->cur_y, priv->cur_z, priv->cur_t);
			}				
		}
		if(event.type & JS_EVENT_AXIS)
		{
			int lock = 0;
			
			if(priv->head_lock)
			{
				lock = priv->cur_t - priv->head_thresh;
				ErrorF("Lock %d\n", lock);
				if(lock < -priv->head_lock || lock > priv->head_lock)
					lock = 0;
				else
					lock = priv->head_button;
			}
				
			switch(event.number)
			{
				case 0:
					if(!lock)
						priv->cur_x = event.value+32768;
					break;
				case 1:
					if(!lock)
						priv->cur_y = event.value+32768;
					break;
				case 2:
					if(priv->button_5 == 0)
					{
						/* We use cur_z to hold cur_t in 
						   head button mode. Hackish but saves
						   a lot of conditional code elsewhere */
						if(priv->head_button)
							priv->cur_z = event.value + 32768;
						else
							priv->cur_t = event.value +32768;
					}
					else
					{
						if(event.value > 0)
							event.value = 1;
						else
							event.value = 0;
						if(priv->buttons[4] != event.value)
						{
							priv->buttons[4] = event.value;
							xf86PostButtonEvent(priv->head->dev, TRUE, 4, event.value,
								0, priv->axes, priv->cur_x, priv->cur_y, priv->cur_z, priv->cur_t);
						}						
					}		
				case 3:
					if(priv->head_button == 0)
						priv->cur_z = event.value+32768;
					else
					{
						ErrorF("Head at %d\n", event.value + 32768);
						priv->cur_t = event.value+32768;
						/* Low is near... */
						if(priv->cur_t < priv->head_thresh)
							event.value = 1;
						else
							event.value = 0;
						if(priv->buttons[0] != event.value)
						{
							priv->buttons[0] = event.value;
							xf86PostButtonEvent(priv->head->dev, TRUE, 1, event.value,
								0, priv->axes, priv->cur_x, priv->cur_y, priv->cur_z, priv->cur_t);
						}						
					}		
					break;
			}
			xf86PostMotionEvent(priv->head->dev, TRUE, 0, priv->axes, priv->cur_x, priv->cur_y, priv->cur_z, priv->cur_t);
		}
	}
}


/*
 ***************************************************************************
 *
 * xf86UR98Control --
 *
 ***************************************************************************
 */
static Bool
xf86UR98Control(DeviceIntPtr	dev,
	       int		mode)
{
	static unsigned char map[] = { 0, 1, 2, 3, 4, 5};
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	UR98PrivatePtr priv = (UR98PrivatePtr)(local->private);
	
	switch(mode)
	{

	case DEVICE_INIT:
		if (priv->screen_no >= screenInfo.numScreens || priv->screen_no < 0) {
			priv->screen_no = 0;
		}
		priv->screen_width = screenInfo.screens[priv->screen_no]->width;
		priv->screen_height = screenInfo.screens[priv->screen_no]->height;

		/*
		 * Device reports button press for up to 5 buttons.
		 */
		if (InitButtonClassDeviceStruct(dev, 4 + priv->button_5, map) == FALSE) {
			ErrorF("Unable to allocate ButtonClassDeviceStruct\n");
			return !Success;
		}
      	
		/*
		 * Device reports motions on 3 axes in absolute coordinates.
		 * Axes min and max values are reported in raw coordinates.
		 * Resolution is computed roughly by the difference between
		 * max and min values scaled from the approximate size of the
		 * screen to fit one meter.
		 */
	
		if (InitValuatorClassDeviceStruct(dev, priv->axes, xf86GetMotionEvents,
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
			if(priv->axes > 2)
			{
				if(priv->head_button)
				{
					/* In head button mode we skip the Z axis */
					InitValuatorAxisStruct(dev, 2, priv->min_t, priv->max_t,
					       65535,	/* resolution */
					       0     /* min_res */,
					       65535 /* max_res */);
				}
				else
				{
					InitValuatorAxisStruct(dev, 2, priv->min_z, priv->max_z,
					       65535,	/* resolution */
					       0     /* min_res */,
					       65535 /* max_res */);
				}
			}
			if(priv->axes > 3)
				InitValuatorAxisStruct(dev, 3, priv->min_t, priv->max_t,
				       65535,	/* resolution */
				       0     /* min_res */,
				       65535 /* max_res */);
		}
	      
		if (InitFocusClassDeviceStruct(dev) == FALSE) {
			ErrorF("Unable to allocate FocusClassDeviceStruct\n");
		}
		
		if(InitPtrFeedbackClassDeviceStruct(dev, UR98PtrCtrl) == FALSE) {
			ErrorF("Unable to allocate PtrFeedBackClassDeviceStruct\n");
		}
	      
		/*
		 * Allocate the motion events buffer.
		 */

		xf86MotionHistoryAllocate(local);
      
		return Success;
    
	case DEVICE_ON:
		if (local->fd < 0) {
			char c;
			int ver;
			
			local->fd = open(priv->input_dev, O_RDONLY|O_NDELAY);
			if (local->fd < 0) {
				ErrorF("Unable to open UR98 headtracker device\n");
				return !Success;
			}
			if(ioctl(local->fd, JSIOCGVERSION, &ver)==-1)
			{
				ErrorF("Unable to query headtracker interface version\n");
				return !Success;
			}
			if(ioctl(local->fd, JSIOCGAXES, &c)==-1)
			{
				ErrorF("Unable to query headtracker parameters\n");
				return !Success;
			}
			if(c!=4)
			{
				ErrorF("Device is not a UR-98\n");
				return !Success;
			}
			if(ioctl(local->fd, JSIOCGBUTTONS, &c)==-1)
			{
				ErrorF("Unable to query headtracker parameters\n");
				return !Success;
			}
			if(c!=4)
			{
				ErrorF("Device is not a UR-98\n");
				return !Success;
			}			
			AddEnabledDevice(local->fd);
		}
		dev->public.on = TRUE; 
		return Success;
      
	case DEVICE_OFF:
		dev->public.on = FALSE;
		return Success;
    
	case DEVICE_CLOSE:
		dev->public.on = FALSE;
		if (local->fd >= 0) {
			xf86RemoveEnabledDevice(local);
			close(local->fd);
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
 * xf86UR98ControlProc --
 *
 ***************************************************************************
 */
 
static int xf86UR98ControlProc (InputInfoPtr pInfo, xDeviceCtl * control)
{
	return (Success);
}

/*
 ***************************************************************************
 *
 * xf86UR98Allocate --
 *
 ***************************************************************************
 */
static LocalDevicePtr xf86UR98Allocate(InputDriverPtr drv, char *name, char *type_name, int flag)
{
	LocalDevicePtr local = xf86AllocateInput(drv, 0);
	UR98PrivatePtr priv = (UR98PrivatePtr) xalloc(sizeof(UR98PrivateRec));
	int i;
  
	if (!local || !priv)
	{
		if(priv)
			xfree(priv);
		if(local)
			xfree(local);
		return NULL;
	}

	priv->input_dev = strdup("/dev/js0");
	priv->min_x = 0;
	priv->max_x = 0;
	priv->min_y = 0;
	priv->max_y = 0;
	priv->min_z = 0;
	priv->max_z = 0;
	priv->min_t = 0;
	priv->max_t = 0;
	priv->button_5 = 0;
	priv->axes = 4;
	priv->screen_no = 0;
	priv->screen_width = -1;
	priv->screen_height = -1;
	priv->inited = 0;
	priv->state = 0;
	priv->swap_axes = 0;
	for(i = 0; i < 5; i++)
		priv->buttons[i] = 0;
	priv->head_button = 0;
	priv->head_thresh = 0;
	priv->head_lock = 0;
	
	local->name = name;
	local->flags = 0 /* XI86_NO_OPEN_ON_INIT */;
	local->device_control = xf86UR98Control;
	local->read_input = xf86UR98ReadInput;
	local->control_proc = xf86UR98ControlProc;
	local->close_proc = NULL;
	local->switch_mode = NULL;
	local->conversion_proc = xf86UR98Convert;
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
 * xf86UR98AllocateHeadTracker --
 *
 ***************************************************************************
 */
static LocalDevicePtr xf86UR98AllocateHeadTracker(InputDriverPtr drv)
{
	LocalDevicePtr local = xf86UR98Allocate(drv, XI_STYLUS, "UR98 HeadTracker", 1);
	if (local)
		((UR98PrivatePtr) local->private)->head = local;
	return local;
}


static void xf86UR98Uninit(InputDriverPtr drv, LocalDevicePtr local, int flags)
{
	UR98PrivatePtr priv = (UR98PrivatePtr) local->private;
  
	xf86UR98Control(local->dev, DEVICE_OFF);

	if (priv) {
		priv->head->private = NULL;
		xfree(priv->input_dev);
		xfree(priv);
	}
	xfree(local->name);
	xfree(local);

	xf86DeleteInput(local, 0);
}

static InputInfoPtr xf86UR98Init(InputDriverPtr drv, IDevPtr dev, int flags)
{
	LocalDevicePtr local=NULL;
	UR98PrivatePtr priv=NULL;
	char *str;
	int portrait=0;


	local = xf86UR98AllocateHeadTracker(drv);
	priv = local->private;
	local->conf_idev = dev;

	xf86CollectInputOptions(local, NULL, NULL);
  
	str = xf86FindOptionValue(local->options, "Device");
	if (!str) {
		xf86Msg(X_ERROR, "%s: No Device specified in UR98 module config.\n", dev->identifier);
		goto init_err;
	}
	priv->input_dev = strdup(str);
	priv->head = local;

	/* Process the common options. */
	xf86ProcessCommonOptions(local, local->options);
  
	str = xf86FindOptionValue(local->options, "DeviceName");
	if (str) {
		local->name = strdup(str);
	}
	xf86Msg(X_CONFIG, "UR98 X device name: %s\n", local->name);  
	priv->screen_no = xf86SetIntOption(local->options, "ScreenNo", 0);
	xf86Msg(X_CONFIG, "UR98 associated screen: %d\n", priv->screen_no);  
	priv->max_x = xf86SetIntOption(local->options, "MaxX", DEFAULT_MAX_X);
	xf86Msg(X_CONFIG, "UR98 maximum x position: %d\n", priv->max_x);
	priv->min_x = xf86SetIntOption(local->options, "MinX", DEFAULT_MIN_X);
	xf86Msg(X_CONFIG, "UR98 minimum x position: %d\n", priv->min_x);
	priv->max_y = xf86SetIntOption(local->options, "MaxY", DEFAULT_MAX_Y);
	xf86Msg(X_CONFIG, "UR98 maximum y position: %d\n", priv->max_y);
	priv->min_y = xf86SetIntOption(local->options, "MinY", DEFAULT_MIN_Y);
	xf86Msg(X_CONFIG, "UR98 minimum y position: %d\n", priv->min_y);
	priv->max_z = xf86SetIntOption(local->options, "MaxZ", DEFAULT_MAX_Z);
	xf86Msg(X_CONFIG, "UR98 maximum z position: %d\n", priv->max_z);
	priv->min_z = xf86SetIntOption(local->options, "MinZ", DEFAULT_MIN_Z);
	xf86Msg(X_CONFIG, "UR98 minimum z position: %d\n", priv->min_z);
	priv->max_t = xf86SetIntOption(local->options, "MaxT", DEFAULT_MAX_T);
	xf86Msg(X_CONFIG, "UR98 maximum z position: %d\n", priv->max_t);
	priv->min_t = xf86SetIntOption(local->options, "MinT", DEFAULT_MIN_T);
	xf86Msg(X_CONFIG, "UR98 minimum z position: %d\n", priv->min_t);
	priv->head_button = xf86SetBoolOption(local->options, "HeadButton", 0);
	if(priv->head_button)
		xf86Msg(X_CONFIG, "UR98 head proximity acts as button 1\n");
	priv->head_thresh = xf86SetIntOption(local->options, "HeadButton", DEFAULT_HEAD_THRESH);
	if(priv->head_button)
		xf86Msg(X_CONFIG, "UR98 proximity threshhold %d\n", priv->head_thresh);
	if(priv->head_button)
	{
		priv->head_lock = xf86SetIntOption(local->options, "HeadLock", DEFAULT_HEAD_LOCK);
		xf86Msg(X_CONFIG, "UR98 proximity lock range %d\n", priv->head_thresh);
	}
	priv->button_5 = xf86SetBoolOption(local->options, "Button5", 0);
	if(priv->button_5)
		xf86Msg(X_CONFIG, "UR98 throttle mapped as button 5\n");
	priv->swap_axes = xf86SetBoolOption(local->options, "SwapXY", 0);
	if (priv->swap_axes) {
		xf86Msg(X_CONFIG, "UR98 %s device will work with X and Y axes swapped\n",
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
		xf86Msg(X_ERROR, "UR98 portrait mode should be: Portrait, Landscape or PortraitCCW");
		str = "Landscape";
	}
	xf86Msg(X_CONFIG, "UR98 device will work in %s mode\n", str);      
  
	if (priv->max_x - priv->min_x <= 0) {
		xf86Msg(X_INFO, "UR98: reverse x mode (minimum x position >= maximum x position)\n");
	}  
	if (priv->max_y - priv->min_y <= 0) {
		xf86Msg(X_INFO, "UR98: reverse y mode (minimum y position >= maximum y position)\n");    
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
	
	/* If we have button_5 set then we lose the 't' axis */
	if(priv->button_5)
		priv->axes = 3;
	else
		priv->axes = 4;
		
	/* If we have head_button set then we lose the 'z' axis */
	if(priv->head_button)
		priv->axes--;

	xf86ProcessCommonOptions(local, local->options);
		
	/* mark the device configured */
	local->flags |= XI86_CONFIGURED;
	return local;

init_err:
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
InputDriverRec UR98 = {
	1,			/* driver version */
	"ur98",			/* driver name */
	NULL,			/* identify */
	xf86UR98Init,		/* pre-init */
	xf86UR98Uninit,		/* un-init */
	NULL,			/* module */
	0			/* ref count */
};

#ifdef XFree86LOADER
static pointer Plug(pointer module, pointer options, int *errmaj,int *errmin)
{
	xf86LoaderReqSymLists(reqSymbols, NULL);
	xf86AddInputDriver(&UR98, module, 0);
	return module;
}

static void Unplug(pointer p)
{
}

static XF86ModuleVersionInfo version_rec = {
	"ur98",
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
XF86ModuleData ur98ModuleData = { &version_rec, Plug, Unplug };
#endif

