/*
 * Copyright 1999 by Frederic Lepied, France. <Lepied@XFree86.org>
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/void/void.c,v 1.2 2000/08/11 19:10:48 dawes Exp $ */

/* Input device which doesn't output any event. This device can be used
 * as a core pointer or as a core keyboard.
 */
#ifndef XFree86LOADER
#include <unistd.h>
#include <errno.h>
#endif

#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#if !defined(DGUX)
#include <xf86_ansic.h>
#include <xisb.h>
#endif
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <exevents.h>		/* Needed for InitValuator/Proximity stuff */
#include <keysym.h>
#include <mipointer.h>

#ifdef XFree86LOADER
#include <xf86Module.h>
#endif

#define MAXBUTTONS 3

/******************************************************************************
 * Function/Macro keys variables
 *****************************************************************************/
static KeySym void_map[] = 
{
    NoSymbol,	/* 0x00 */
    NoSymbol,	/* 0x01 */
    NoSymbol,	/* 0x02 */
    NoSymbol,	/* 0x03 */
    NoSymbol,	/* 0x04 */
    NoSymbol,	/* 0x05 */
    NoSymbol,	/* 0x06 */
    NoSymbol	/* 0x07 */
};

/* minKeyCode = 8 because this is the min legal key code */
static KeySymsRec void_keysyms = {
  /* map	minKeyCode	maxKC	width */
  void_map,	8,		8,	1
};

static const char *DEFAULTS[] = {
    NULL
};

/*
 * xf86VoidControlProc --
 *
 * called to change the state of a device.
 */
static int
xf86VoidControlProc(DeviceIntPtr device, int what)
{
    InputInfoPtr pInfo;
    unsigned char map[MAXBUTTONS + 1];
    int i;
    
    pInfo = device->public.devicePrivate;
    
    switch (what)
    {
    case DEVICE_INIT:
	device->public.on = FALSE;

	for (i = 0; i < MAXBUTTONS; i++) {
	    map[i + 1] = i + 1;
	}
	
	if (InitButtonClassDeviceStruct(device,
					MAXBUTTONS,
					map) == FALSE) {
	  ErrorF("unable to allocate Button class device\n");
	  return !Success;
	}
      
	if (InitFocusClassDeviceStruct(device) == FALSE) {
	  ErrorF("unable to init Focus class device\n");
	  return !Success;
	}
          
	if (InitKeyClassDeviceStruct(device, &void_keysyms, NULL) == FALSE) {
	  ErrorF("unable to init key class device\n"); 
	  return !Success;
	}

	if (InitValuatorClassDeviceStruct(device, 
					  2,
					  xf86GetMotionEvents, 
					  0,
					  Absolute) == FALSE) {
	  InitValuatorAxisStruct(device,
				 0,
				 0, /* min val */1, /* max val */
				 1, /* resolution */
				 0, /* min_res */
				 1); /* max_res */
	  InitValuatorAxisStruct(device,
				 1,
				 0, /* min val */1, /* max val */
				 1, /* resolution */
				 0, /* min_res */
				 1); /* max_res */
	  ErrorF("unable to allocate Valuator class device\n"); 
	  return !Success;
	}
	else {
	  /* allocate the motion history buffer if needed */
	  xf86MotionHistoryAllocate(pInfo);
	}
	break;

    case DEVICE_ON:
	device->public.on = TRUE;
	break;

    case DEVICE_OFF:
    case DEVICE_CLOSE:
	device->public.on = FALSE;
	break;
    }
    return Success;
}

/*
 * xf86VoidUninit --
 *
 * called when the driver is unloaded.
 */
static void
xf86VoidUninit(InputDriverPtr	drv,
	       InputInfoPtr	pInfo,
	       int		flags)
{
    xf86VoidControlProc(pInfo->dev, DEVICE_OFF);
}

/*
 * xf86VoidInit --
 *
 * called when the module subsection is found in XF86Config
 */
static InputInfoPtr
xf86VoidInit(InputDriverPtr	drv,
	     IDevPtr		dev,
	     int		flags)
{
    InputInfoPtr pInfo;

    if (!(pInfo = xf86AllocateInput(drv, 0)))
	return NULL;

    /* Initialise the InputInfoRec. */
    pInfo->name = dev->identifier;
    pInfo->type_name = "Void";
    pInfo->flags = XI86_KEYBOARD_CAPABLE | XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
    pInfo->device_control = xf86VoidControlProc;
    pInfo->read_input = NULL;
    pInfo->motion_history_proc = xf86GetMotionEvents;
    pInfo->history_size = 0;
    pInfo->control_proc = NULL;
    pInfo->close_proc = NULL;
    pInfo->switch_mode = NULL;
    pInfo->conversion_proc = NULL;
    pInfo->reverse_conversion_proc = NULL;
    pInfo->fd = -1;
    pInfo->dev = NULL;
    pInfo->private_flags = 0;
    pInfo->always_core_feedback = 0;
    pInfo->conf_idev = dev;

    /* Collect the options, and process the common options. */
    xf86CollectInputOptions(pInfo, DEFAULTS, NULL);
    xf86ProcessCommonOptions(pInfo, pInfo->options);
    
    /* Mark the device configured */
    pInfo->flags |= XI86_CONFIGURED;

    /* Return the configured device */
    return (pInfo);
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec VOID = {
    1,				/* driver version */
    "void",			/* driver name */
    NULL,			/* identify */
    xf86VoidInit,		/* pre-init */
    xf86VoidUninit,		/* un-init */
    NULL,			/* module */
    0				/* ref count */
};

/*
 ***************************************************************************
 *
 * Dynamic loading functions
 *
 ***************************************************************************
 */
#ifdef XFree86LOADER
/*
 * xf86VoidUnplug --
 *
 * called when the module subsection is found in XF86Config
 */
static void
xf86VoidUnplug(pointer	p)
{
}

/*
 * xf86VoidPlug --
 *
 * called when the module subsection is found in XF86Config
 */
static pointer
xf86VoidPlug(pointer	module,
	    pointer	options,
	    int		*errmaj,
	    int		*errmin)
{
    xf86AddInputDriver(&VOID, module, 0);

    return module;
}

static XF86ModuleVersionInfo xf86VoidVersionRec =
{
    "void",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    1, 0, 0,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}		/* signature, to be patched into the file by */
				/* a tool */
};

XF86ModuleData voidModuleData = {&xf86VoidVersionRec,
				  xf86VoidPlug,
				  xf86VoidUnplug};

#endif /* XFree86LOADER */

/*
 * Local variables:
 * change-log-default-name: "~/xinput.log"
 * c-file-style: "bsd"
 * End:
 */
/* end of void.c */
