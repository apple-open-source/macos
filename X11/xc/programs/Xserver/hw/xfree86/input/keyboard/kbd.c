/* $XFree86: xc/programs/Xserver/hw/xfree86/input/keyboard/kbd.c,v 1.9 2003/12/18 21:53:45 dawes Exp $ */

/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 * Author: Ivan Pascal.
 *
 * Based on the code from
 * xf86Config.c which is
 * Copyright 1991-2002 by The XFree86 Project, Inc.
 * Copyright 1997 by Metro Link, Inc.
 * xf86Events.c and xf86Io.c which are
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 */
  
#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"

#include "xf86.h"
#include "atKeynames.h"
#include "xf86Privstr.h"

#ifdef XINPUT
#include "XI.h"
#include "XIproto.h"
#include "extnsionst.h"
#include "extinit.h"
#else
#include "inputstr.h"
#endif

#include "xf86Xinput.h"
#include "xf86_OSproc.h"
#include "xf86OSKbd.h"
#include "xf86_ansic.h"
#include "compiler.h"

#ifdef XKB
#include <X11/extensions/XKB.h>
#include <X11/extensions/XKBstr.h>
#include <X11/extensions/XKBsrv.h>
#endif

#define CAPSFLAG	1
#define NUMFLAG		2
#define SCROLLFLAG	4
#define MODEFLAG	8
#define COMPOSEFLAG	16

static InputInfoPtr KbdPreInit(InputDriverPtr drv, IDevPtr dev, int flags);
static int KbdProc(DeviceIntPtr device, int what);
static int KbdCtrl(DeviceIntPtr device, KeybdCtrl *ctrl);
static void KbdBell(int percent, DeviceIntPtr dev, pointer ctrl, int unused);
static void PostKbdEvent(InputInfoPtr pInfo, unsigned int key, Bool down);

static void InitKBD(InputInfoPtr pInfo, Bool init);
static void SetXkbOption(InputInfoPtr pInfo, char *name, char **option);
static void UpdateLeds(InputInfoPtr pInfo);

#undef KEYBOARD
InputDriverRec KEYBOARD = {
	1,
	"kbd",
	NULL,
	KbdPreInit,
	NULL,
	NULL,
	0
};

typedef enum {
    OPTION_ALWAYS_CORE,
    OPTION_SEND_CORE_EVENTS,
    OPTION_CORE_KEYBOARD,
    OPTION_DEVICE,
    OPTION_PROTOCOL,
    OPTION_AUTOREPEAT,
    OPTION_XLEDS,
    OPTION_XKB_DISABLE,
    OPTION_XKB_KEYMAP,
    OPTION_XKB_KEYCODES,
    OPTION_XKB_TYPES,
    OPTION_XKB_COMPAT,
    OPTION_XKB_SYMBOLS,
    OPTION_XKB_GEOMETRY,
    OPTION_XKB_RULES,
    OPTION_XKB_MODEL,
    OPTION_XKB_LAYOUT,
    OPTION_XKB_VARIANT,
    OPTION_XKB_OPTIONS,
    OPTION_PANIX106,
    OPTION_CUSTOM_KEYCODES
} KeyboardOpts;

#ifdef XFree86LOADER
/* These aren't actually used ... */
static const OptionInfoRec KeyboardOptions[] = {
    { OPTION_ALWAYS_CORE,	"AlwaysCore",	  OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SEND_CORE_EVENTS,	"SendCoreEvents", OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_CORE_KEYBOARD,	"CoreKeyboard",	  OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_DEVICE,		"Device",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_PROTOCOL,		"Protocol",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_AUTOREPEAT,	"AutoRepeat",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XLEDS,		"XLeds",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_DISABLE,	"XkbDisable",	  OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_XKB_KEYMAP,	"XkbKeymap",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_KEYCODES,	"XkbKeycodes",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_TYPES,		"XkbTypes",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_COMPAT,	"XkbCompat",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_SYMBOLS,	"XkbSymbols",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_GEOMETRY,	"XkbGeometry",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_RULES,		"XkbRules",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_MODEL,		"XkbModel",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_LAYOUT,	"XkbLayout",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_VARIANT,	"XkbVariant",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_XKB_OPTIONS,	"XkbOptions",	  OPTV_STRING,	{0}, FALSE },
    { OPTION_PANIX106,		"Panix106",	  OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_CUSTOM_KEYCODES,   "CustomKeycodes", OPTV_BOOLEAN,	{0}, FALSE },
    { -1,			NULL,		  OPTV_NONE,	{0}, FALSE }
};
#endif

static const char *kbdDefaults[] = {
    "Protocol",		"standard",
    "AutoRepeat",	"500 30",
    "XkbRules",		"xfree86",
    "XkbModel",		"pc101",
    "XkbLayout",	"us",
    "Panix106",		"off",
    "CustomKeycodes",	"off",
    NULL
};

static const char *kbd98Defaults[] = {
    "Protocol",		"standard",
    "AutoRepeat",	"500 30",
    "XkbRules",		"xfree98",
    "XkbModel",		"pc98",
    "XkbLayout",	"nec/jp",
    "Panix106",		"off",
    "CustomKeycodes",	"off",
    NULL
};

#ifdef XKB
static char *xkb_rules;
static char *xkb_model;
static char *xkb_layout;
static char *xkb_variant;
static char *xkb_options;

static XkbComponentNamesRec xkbnames;
#endif /* XKB */

#ifdef XFree86LOADER
/*ARGSUSED*/
static const OptionInfoRec *
KeyboardAvailableOptions(void *unused)
{
    return (KeyboardOptions);
}
#endif

static void
SetXkbOption(InputInfoPtr pInfo, char *name, char **option)
{
   char *s;

   if ((s = xf86SetStrOption(pInfo->options, name, NULL))) {
       if (!s[0]) {
           xfree(s);
           *option = NULL;
       } else {
           *option = s;
           xf86Msg(X_CONFIG, "%s: %s: \"%s\"\n", pInfo->name, name, s);
       }
    }
}

static InputInfoPtr
KbdPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    InputInfoPtr pInfo;
    KbdDevPtr pKbd;
    MessageType from = X_DEFAULT;
    char *s;

    if (!(pInfo = xf86AllocateInput(drv, 0)))
	return NULL;

    /* Initialise the InputInfoRec. */
    pInfo->name = dev->identifier;
    pInfo->type_name = XI_KEYBOARD;
    pInfo->flags = XI86_KEYBOARD_CAPABLE;
    pInfo->device_control = KbdProc;
    pInfo->read_input = NULL;
    pInfo->motion_history_proc = NULL;
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

    if (!xf86IsPc98())
        xf86CollectInputOptions(pInfo, kbdDefaults, NULL);
    else
        xf86CollectInputOptions(pInfo, kbd98Defaults, NULL);
    xf86ProcessCommonOptions(pInfo, pInfo->options); 

    if (!(pKbd = xcalloc(sizeof(KbdDevRec), 1)))
        return pInfo;

    pInfo->private = pKbd;
    pKbd->PostEvent = PostKbdEvent;

    if (!xf86OSKbdPreInit(pInfo))
        return pInfo;

    if (!pKbd->OpenKeyboard(pInfo)) {
        return pInfo;
    }

    if ((s = xf86SetStrOption(pInfo->options, "AutoRepeat", NULL))) {
        int delay, rate;
        if (sscanf(s, "%d %d", &delay, &rate) != 2) {
            xf86Msg(X_ERROR, "\"%s\" is not a valid AutoRepeat value", s);
        } else {
            pKbd->delay = delay;
            pKbd->rate = rate;
        }
        xfree(s);
    }

    if ((s = xf86SetStrOption(pInfo->options, "XLeds", NULL))) {
        char *l, *end;
        unsigned int i;
        l = strtok(s, " \t\n");
        while (l) {
    	    i = strtoul(l, &end, 0);
    	    if (*end == '\0')
    	        pKbd->xledsMask |= 1L << (i - 1);
    	    else {
    	        xf86Msg(X_ERROR, "\"%s\" is not a valid XLeds value", l);
    	    }
    	    l = strtok(NULL, " \t\n");
        }
        xfree(s);
    }

#ifdef XKB

/* XkbDisable must be a server flag but for compatibility we check it here */

  if (xf86FindOption(pInfo->options, "XkbDisable"))
      xf86Msg(X_WARNING,
             "%s: XKB can't be disabled here. Use \"ServerFlags\" section.\n",
              pInfo->name);

  pKbd->noXkb = noXkbExtension;
  if (pKbd->noXkb) {
      xf86Msg(X_CONFIG, "XKB: disabled\n");
  } else {
      SetXkbOption(pInfo, "XkbKeymap", &xkbnames.keymap);
      if (xkbnames.keymap) {
          xf86Msg(X_CONFIG, "%s: XkbKeymap overrides all other XKB settings\n",
                  pInfo->name);
      } else {
          SetXkbOption(pInfo, "XkbRules", &xkb_rules);
          SetXkbOption(pInfo, "XkbModel", &xkb_model);
          SetXkbOption(pInfo, "XkbLayout", &xkb_layout);
          SetXkbOption(pInfo, "XkbVariant", &xkb_variant);
          SetXkbOption(pInfo, "XkbOptions", &xkb_options);

          SetXkbOption(pInfo, "XkbKeycodes", &xkbnames.keycodes);
          SetXkbOption(pInfo, "XkbTypes", &xkbnames.types);
          SetXkbOption(pInfo, "XkbCompat", &xkbnames.compat);
          SetXkbOption(pInfo, "XkbSymbols", &xkbnames.symbols);
          SetXkbOption(pInfo, "XkbGeometry", &xkbnames.geometry);
      }
  }

  if ((xkb_model && !strcmp(xkb_model, "sun")) ||
      (xkb_rules && !strcmp(xkb_rules, "sun")))
       pKbd->sunKbd = TRUE;
#endif

#if defined(SVR4) && defined(i386)
  if ((pKbd->Panix106 =
      xf86SetBoolOption(pInfo->options, "Panix106", FALSE))) {
      xf86Msg(X_CONFIG, "%s: PANIX106: enabled\n", pInfo->name);
  }
#endif

  pKbd->CustomKeycodes = FALSE;
  from = X_DEFAULT; 
  if (xf86FindOption(pInfo->options, "CustomKeycodes")) {
      pKbd->CustomKeycodes = xf86SetBoolOption(pInfo->options, "CustomKeycodes",
                                               pKbd->CustomKeycodes);
     from = X_CONFIG;
  }

  xf86Msg(from, "%s: CustomKeycodes %s\n",
               pInfo->name, pKbd->CustomKeycodes ? "enabled" : "disabled");

  pInfo->flags |= XI86_CONFIGURED;

  return pInfo;
}

static void
KbdBell(int percent, DeviceIntPtr dev, pointer ctrl, int unused)
{
   InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
   KbdDevPtr pKbd = (KbdDevPtr) pInfo->private;
   pKbd->Bell(pInfo, percent, ((KeybdCtrl*) ctrl)->bell_pitch,
                              ((KeybdCtrl*) ctrl)->bell_duration);
}

static void
UpdateLeds(InputInfoPtr pInfo)
{
    KbdDevPtr pKbd = (KbdDevPtr) pInfo->private;
    int leds = 0;

    if (pKbd->keyLeds & CAPSFLAG)    leds |= XLED1;
    if (pKbd->keyLeds & NUMFLAG)     leds |= XLED2;
    if (pKbd->keyLeds & SCROLLFLAG ||
        pKbd->keyLeds & MODEFLAG)    leds |= XLED3;
    if (pKbd->keyLeds & COMPOSEFLAG) leds |= XLED4;

    pKbd->leds = (pKbd->leds & pKbd->xledsMask) | (leds & ~pKbd->xledsMask);
    pKbd->SetLeds(pInfo, pKbd->leds);
}

static int
KbdCtrl( DeviceIntPtr device, KeybdCtrl *ctrl)
{
   int leds;
   InputInfoPtr pInfo = (InputInfoPtr) device->public.devicePrivate;
   KbdDevPtr pKbd = (KbdDevPtr) pInfo->private;

   if ( ctrl->leds & XCOMP ) {
       pKbd->keyLeds |= COMPOSEFLAG;
   } else {
       pKbd->keyLeds &= ~COMPOSEFLAG;
   }
   leds = ctrl->leds & ~(XCAPS | XNUM | XSCR); /* ??? */
#ifdef XKB
   if (pKbd->noXkb) {
#endif
       pKbd->leds = (leds & pKbd->xledsMask) | (pKbd->leds & ~pKbd->xledsMask);
#ifdef XKB
  } else {
       pKbd->leds = leds;
  }
#endif
  pKbd->SetLeds(pInfo, pKbd->leds);

  return (Success);
}

static void
InitKBD(InputInfoPtr pInfo, Bool init)
{
  char            rad;
  unsigned int    i;
  xEvent          kevent;
  KbdDevPtr pKbd = (KbdDevPtr) pInfo->private;
  DeviceIntPtr    pKeyboard = pInfo->dev;
  KeyClassRec     *keyc = pKeyboard->key;
  KeySym          *map = keyc->curKeySyms.map;

  kevent.u.keyButtonPointer.time = GetTimeInMillis();
  kevent.u.keyButtonPointer.rootX = 0;
  kevent.u.keyButtonPointer.rootY = 0;

  /*
   * Hmm... here is the biggest hack of every time !
   * It may be possible that a switch-vt procedure has finished BEFORE
   * you released all keys neccessary to do this. That peculiar behavior
   * can fool the X-server pretty much, cause it assumes that some keys
   * were not released. TWM may stuck alsmost completly....
   * OK, what we are doing here is after returning from the vt-switch
   * exeplicitely unrelease all keyboard keys before the input-devices
   * are reenabled.
   */
  for (i = keyc->curKeySyms.minKeyCode, map = keyc->curKeySyms.map;
       i < keyc->curKeySyms.maxKeyCode;
       i++, map += keyc->curKeySyms.mapWidth)
     if (KeyPressed(i))
      {
        switch (*map) {
        /* Don't release the lock keys */
        case XK_Caps_Lock:
        case XK_Shift_Lock:
        case XK_Num_Lock:
        case XK_Scroll_Lock:
        case XK_Kana_Lock:
          break;
        default:
          kevent.u.u.detail = i;
          kevent.u.u.type = KeyRelease;
          (* pKeyboard->public.processInputProc)(&kevent, pKeyboard, 1);
        }
      }
  pKbd->scanPrefix      = 0;

  if (init) {
      pKbd->keyLeds = 0;

      UpdateLeds(pInfo);

      if( pKbd->delay <= 375) rad = 0x00;
      else if (pKbd->delay <= 625) rad = 0x20;
      else if (pKbd->delay <= 875) rad = 0x40;
      else                         rad = 0x60;
      if      (pKbd->rate <=  2)   rad |= 0x1F;
      else if (pKbd->rate >= 30)   rad |= 0x00;
      else                         rad |= ((58 / pKbd->rate) - 2);
      pKbd->SetKbdRepeat(pInfo, rad);
  } else
      UpdateLeds(pInfo);
}

static int
KbdProc(DeviceIntPtr device, int what)
{

  InputInfoPtr pInfo = device->public.devicePrivate;
  KbdDevPtr pKbd = (KbdDevPtr) pInfo->private;
  KeySymsRec           keySyms;
  CARD8                modMap[MAP_LENGTH];
  int                  ret;

  switch (what) {
     case DEVICE_INIT:
        ret = pKbd->KbdInit(pInfo, what);
	if (ret != Success)
	    return ret;

        pKbd->KbdGetMapping(pInfo, &keySyms, modMap);

        device->public.on = FALSE;
#ifdef XKB
        if (pKbd->noXkb) {
#endif
            InitKeyboardDeviceStruct((DevicePtr) device,
                             &keySyms,
                             modMap,
                             KbdBell,
                             (KbdCtrlProcPtr)KbdCtrl);
#ifdef XKB
        } else {
            if (xkbnames.keymap)
                xkb_rules = NULL;
            XkbSetRulesDflts(xkb_rules, xkb_model, xkb_layout,
                             xkb_variant, xkb_options);
            XkbInitKeyboardDeviceStruct(device,
                                        &xkbnames,
                                        &keySyms,
                                        modMap,
                                        KbdBell,
                                        (KbdCtrlProcPtr)KbdCtrl);
    }
#endif
    InitKBD(pInfo, TRUE);
    break;
  case DEVICE_ON:
    if (device->public.on)
	break;
    /*
     * Set the keyboard into "direct" mode and turn on
     * event translation.
     */
    if ((ret = pKbd->KbdOn(pInfo, what)) != Success)
	return ret;
    /*
     * Discard any pending input after a VT switch to prevent the server
     * passing on parts of the VT switch sequence.
     */
    if (pInfo->fd >= 0) {
	sleep(1);
	xf86FlushInput(pInfo->fd);
	AddEnabledDevice(pInfo->fd);
    }

    device->public.on = TRUE;
    InitKBD(pInfo, FALSE);
    break;

  case DEVICE_CLOSE:
  case DEVICE_OFF:

    /*
     * Restore original keyboard directness and translation.
     */
    if (pInfo->fd != -1)
      RemoveEnabledDevice(pInfo->fd);
    pKbd->KbdOff(pInfo, what);
    device->public.on = FALSE;
    break;
  }
  return (Success);
}

static void
PostKbdEvent(InputInfoPtr pInfo, unsigned int scanCode, Bool down)
{

  KbdDevPtr    pKbd = (KbdDevPtr) pInfo->private;
  DeviceIntPtr device = pInfo->dev;
  KeyClassRec  *keyc = device->key;
  KbdFeedbackClassRec *kbdfeed = device->kbdfeed;
  int          specialkey = 0;

  Bool        UsePrefix = FALSE;
  KeySym      *keysym;
  int         keycode;
  unsigned long changeLock = 0;
  static int  lockkeys = 0;

  /* Disable any keyboard processing while in suspend */
  if (xf86inSuspend)
      return;

  /*
   * First do some special scancode remapping ...
   */
  if (pKbd->RemapScanCode != NULL) {
     if (pKbd->RemapScanCode(pInfo, (int*) &scanCode))
         return;
  } else {
     if (pKbd->scancodeMap != NULL) {
         TransMapPtr map = pKbd->scancodeMap; 
         if (scanCode >= map->begin && scanCode < map->end)
             scanCode = map->map[scanCode - map->begin];
     }
  }

  /*
   * and now get some special keysequences
   */

  specialkey = scanCode;

  if (pKbd->GetSpecialKey != NULL) {
     specialkey = pKbd->GetSpecialKey(pInfo, scanCode);
  } else {
     if (pKbd->specialMap != NULL) {
         TransMapPtr map = pKbd->specialMap; 
         if (scanCode >= map->begin && scanCode < map->end)
             specialkey = map->map[scanCode - map->begin];
     }
  }

#ifndef TERMINATE_FALLBACK
#define TERMINATE_FALLBACK 1
#endif
#ifdef XKB
  if (noXkbExtension
#if TERMINATE_FALLBACK
      || specialkey == KEY_BackSpace
#endif
     )
#endif
  {    
      if (xf86CommonSpecialKey(specialkey, down, keyc->state))
	  return;
      if (pKbd->SpecialKey != NULL)
	  if (pKbd->SpecialKey(pInfo, specialkey, down, keyc->state))
	      return;
  }
  
  /*
   * Now map the scancodes to real X-keycodes ...
   */
  keycode = scanCode + MIN_KEYCODE;
  keysym = (keyc->curKeySyms.map +
	    keyc->curKeySyms.mapWidth * 
	    (keycode - keyc->curKeySyms.minKeyCode));

#ifdef XKB
  if (pKbd->noXkb) {
#endif
  /*
   * Filter autorepeated caps/num/scroll lock keycodes.
   */
  if( down ) {
    switch( keysym[0] ) {
        case XK_Caps_Lock :
          if (lockkeys & CAPSFLAG)
              return;
	  else
	      lockkeys |= CAPSFLAG;
          break;

        case XK_Num_Lock :
          if (lockkeys & NUMFLAG)
              return;
	  else
	      lockkeys |= NUMFLAG;
          break;

        case XK_Scroll_Lock :
          if (lockkeys & SCROLLFLAG)
              return;
	  else
	      lockkeys |= SCROLLFLAG;
          break;
    }
    if (keysym[1] == XF86XK_ModeLock)
    {
      if (lockkeys & MODEFLAG)
          return;
      else
          lockkeys |= MODEFLAG;
    }
  }
  else {
    switch( keysym[0] ) {
        case XK_Caps_Lock :
            lockkeys &= ~CAPSFLAG;
            break;

        case XK_Num_Lock :
            lockkeys &= ~NUMFLAG;
            break;

        case XK_Scroll_Lock :
            lockkeys &= ~SCROLLFLAG;
            break;
    }
    if (keysym[1] == XF86XK_ModeLock)
      lockkeys &= ~MODEFLAG;
  }

  /*
   * LockKey special handling:
   * ignore releases, toggle on & off on presses.
   * Don't deal with the Caps_Lock keysym directly, but check the lock modifier
   */

   if (keyc->modifierMap[keycode] & LockMask)
       changeLock = CAPSFLAG;
   if (keysym[0] == XK_Num_Lock)
       changeLock = NUMFLAG;
   if (keysym[0] == XK_Scroll_Lock)
       changeLock = SCROLLFLAG;
   if (keysym[1] == XF86XK_ModeLock)
       changeLock = MODEFLAG;

   if (changeLock) {
      if (!down)
          return;

      pKbd->keyLeds &= ~changeLock;

      if (KeyPressed(keycode)) {
	  down = !down;
      } else {
          pKbd->keyLeds |= changeLock;
      }
      UpdateLeds(pInfo);
  }

  if (!pKbd->CustomKeycodes) {
    /*
     * normal, non-keypad keys
     */
    if (scanCode < KEY_KP_7 || scanCode > KEY_KP_Decimal) {
#if !defined(CSRG_BASED) && \
    !defined(__GNU__) && \
     defined(KB_84)
      /*
       * magic ALT_L key on AT84 keyboards for multilingual support
       */
      if (pKbd->kbdType == KB_84 &&
	  ModifierDown(AltMask) &&
	  keysym[2] != NoSymbol)
	{
	  UsePrefix = TRUE;
	}
#endif /* !CSRG_BASED && ... */
    }
  }
#ifdef XKB
  }
#endif

  /*
   * check for an autorepeat-event
   */
  if (down) {
      int num = keycode >> 3;
      int bit = 1 << (keycode & 7);
      if ((keyc->down[num] & bit) &&
          ((kbdfeed->ctrl.autoRepeat != AutoRepeatModeOn) ||
            keyc->modifierMap[keycode] ||
            !(kbdfeed->ctrl.autoRepeats[num] & bit)))
          return;
  }

   if (UsePrefix) {
      xf86PostKeyboardEvent(device,
              keyc->modifierKeyMap[keyc->maxKeysPerModifier*7], TRUE);
      xf86PostKeyboardEvent(device, keycode, down);
      xf86PostKeyboardEvent(device,
              keyc->modifierKeyMap[keyc->maxKeysPerModifier*7], FALSE);
   } else {
      xf86PostKeyboardEvent(device, keycode, down);
   }
}

#ifdef XFree86LOADER
ModuleInfoRec KeyboardInfo = {
    1,
    "KBD",
    NULL,
    0,
    KeyboardAvailableOptions,
};

static void
xf86KbdUnplug(pointer	p)
{
}

static pointer
xf86KbdPlug(pointer	module,
	    pointer	options,
	    int		*errmaj,
	    int		*errmin)
{
    static Bool Initialised = FALSE;

    if (!Initialised) {
	Initialised = TRUE;
#ifndef REMOVE_LOADER_CHECK_MODULE_INFO
	if (xf86LoaderCheckSymbol("xf86AddModuleInfo"))
#endif
	xf86AddModuleInfo(&KeyboardInfo, module);
    }

    xf86AddInputDriver(&KEYBOARD, module, 0);

    return module;
}

static XF86ModuleVersionInfo xf86KeyboardVersionRec =
{
    "kbd",
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

XF86ModuleData kbdModuleData = {&xf86KeyboardVersionRec,
				     xf86KbdPlug,
				     xf86KbdUnplug};

#endif /* XFree86LOADER */
