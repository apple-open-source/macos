//
//  IOHIDKeyboardEventTranslation.c
//  IOHIDFamily
//
//  Created by yg on 12/4/15.
//
//

#include <dispatch/dispatch.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/hid/IOHIDServiceClient.h>
#include <IOKit/hid/IOHIDService.h>
#include "IOHIDKeyboardEventTranslation.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include "AppleHIDUsageTables.h"
#include "IOHIDFamilyPrivate.h"
#include <IOKit/usb/USB.h>
#include "IOHIDEventData.h"
#include "ev_keymap.h"

#define NX_UNKNOWNKEY 0xff

typedef struct {
  uint32_t                usagePage;
  uint32_t                usage;
  uint32_t                modifierMask;
  uint32_t                stickyUpType;
  uint32_t                stickyDownType;
  uint32_t                stickyLockType;
} MODIFIER_INFO;


enum {
  kSkipKeyboardEventTranslation         = 0x1,
  kSkipFlagchangeEventTranslation       = 0x2,
  kSkipAuxEventTranslation              = 0x4,
  kSkipSystemManagmentEventTranslation  = 0x8,
  kSkipSlowKeyEndEventTranslation       = 0x10
};

typedef struct {
  uint32_t                usage;
  uint32_t                usagePage;
  uint32_t                flags;
  boolean_t               down;
  uint64_t                timestamp;
  uint64_t                serviceid;
  const MODIFIER_INFO     *modifier;
  uint32_t                translationFlags;
  uint32_t                eventFlags;
  uint32_t                skipTranslationPhase;
  CFMutableArrayRef       collection;
  IOHIDEventRef           event;
} EVENT_TRANSLATOR_CONTEXT;

#define ADB_POWER_KEY_CODE     0x7f
#define ADB_CAPSLOCK_KEY_CODE  0x39


typedef CFTypeRef (*FServiceCopyProperty) (void * service, CFStringRef key);

static CFStringRef __IOHIDKeyboardEventTranslatorCopyDebugDescription(CFTypeRef cf);
static IOHIDKeyboardEventTranslatorRef __IOHIDKeyboardEventTranslatorCreatePrivate(CFAllocatorRef allocator, CFAllocatorContext * context __unused);
static void __IOHIDKeyboardEventTranslatorFree( CFTypeRef object );
static uint8_t __IOHIDKeyboardEventTranslatorGetKeyboardKeyCode (uint32_t  usage, uint32_t  usagePage, boolean_t isISO);
static uint8_t __IOHIDKeyboardEventTranslatorGetConsumerKeyCode (uint32_t  usage, uint32_t  usagePage);
static uint16_t __IOHIDKeyboardEventTranslatorIsISOKeyboard (uint32_t keyboardID);

static void __IOHIDKeyboardEventTranslatorInit( IOHIDKeyboardEventTranslatorRef translator , void * service, FServiceCopyProperty propertyFunc);
static uint32_t __IOHIDKeyboardEventTranslatorGetNonModifierFlags (uint32_t  usage, uint32_t  usagePage);
void __IOHIDKeyboardEventProcessModifiers (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context);
void __IOHIDKeyboardEventTranslatorAddAuxButtonEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint8_t keyCode);
CFTypeRef __IOHIDKeyboardEventTranslatorCopyServiceProperty (CFTypeRef  service, CFStringRef key);
void __IOHIDKeyboardEventTranslatorInitNxEvent (EVENT_TRANSLATOR_CONTEXT *context, NXEventExt *nxEvent, uint8_t type);
void __IOHIDKeyboardEventTranslatorAddKeyboardEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint8_t keyCode);
void __IOHIDKeyboardEventTranslatorAddFlagsChangeEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint32_t flags);
void __IOHIDKeyboardEventTranslatorAddStickyKeyEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context);
void __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint16_t subtype);
void __IOHIDKeyboardEventTranslatorAddSlowKeyPhaseEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context);
void __IOHIDKeyboardEventTranslatorAddMenuButtonEvent (IOHIDKeyboardEventTranslatorRef translator , EVENT_TRANSLATOR_CONTEXT *context);
void __IOHIDKeyboardEventTranslatorAddConsumerEvents (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint8_t keyCode);

extern  unsigned int hid_usb_2_adb_keymap[];
extern  unsigned int hid_usb_2_adb_keymap_length;
extern  unsigned int hid_usb_apple_2_adb_keymap[];
extern  unsigned int hid_usb_apple_2_adb_keymap_length;

typedef struct  __IOHIDKeyboardEventTranslator {
  CFRuntimeBase     cfBase;   // base CFType information
  uint32_t          keyboardID;
  uint32_t          eventFlags;
  boolean_t         isISO;
} __IOHIDKeyboardEventTranslator;

#define kNXFlagsModifierMask (NX_COMMANDMASK|NX_ALTERNATEMASK|NX_CONTROLMASK|NX_SHIFTMASK|NX_SECONDARYFNMASK)

static const CFRuntimeClass __IOHIDKeyboardEventTranslatorClass = {
  0,                      // version
  "IOHIDKeyboardEventTranslator",  // className
  NULL,                   // init
  NULL,                   // copy
  __IOHIDKeyboardEventTranslatorFree,     // finalize
  NULL,                   // equal
  NULL,                   // hash
  NULL,                   // copyFormattingDesc
  __IOHIDKeyboardEventTranslatorCopyDebugDescription,
  NULL,
  NULL
};

static dispatch_once_t  __keyboardTranslatorTypeInit            = 0;
static CFTypeID         __keyboardTranslatorTypeID              = _kCFRuntimeNotATypeID;

//------------------------------------------------------------------------------
// IOHIDKeyboardEventTranslatorGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOHIDKeyboardEventTranslatorGetTypeID(void)
{
  if ( _kCFRuntimeNotATypeID == __keyboardTranslatorTypeID ) {
    dispatch_once(&__keyboardTranslatorTypeInit, ^{
      __keyboardTranslatorTypeID = _CFRuntimeRegisterClass(&__IOHIDKeyboardEventTranslatorClass);
    });
  }
  return __keyboardTranslatorTypeID;
}


//------------------------------------------------------------------------------
// IOHIDKeyboardEventTranslatorCreateWithService
//------------------------------------------------------------------------------
IOHIDKeyboardEventTranslatorRef IOHIDKeyboardEventTranslatorCreateWithService(CFAllocatorRef allocator, IOHIDServiceRef service)
{
  IOHIDKeyboardEventTranslatorRef translator  = __IOHIDKeyboardEventTranslatorCreatePrivate(allocator, NULL);
  if (!translator) {
    return translator;
  }
  
  __IOHIDKeyboardEventTranslatorInit (translator, service, (FServiceCopyProperty)IOHIDServiceCopyProperty);
  
  return translator;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardEventTranslatorCreateWithServiceClient
//------------------------------------------------------------------------------
IOHIDKeyboardEventTranslatorRef IOHIDKeyboardEventTranslatorCreateWithServiceClient(CFAllocatorRef allocator, IOHIDServiceClientRef service)
{
  IOHIDKeyboardEventTranslatorRef translator  = __IOHIDKeyboardEventTranslatorCreatePrivate(allocator, NULL);
  if (!translator) {
    return translator;
  }
  
  __IOHIDKeyboardEventTranslatorInit (translator, service, (FServiceCopyProperty)IOHIDServiceClientCopyProperty);

  return translator;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorCreatePrivate
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDKeyboardEventTranslatorRef __IOHIDKeyboardEventTranslatorCreatePrivate(CFAllocatorRef allocator, CFAllocatorContext * context __unused)
{
  IOHIDKeyboardEventTranslatorRef translator = NULL;
  void *                          offset  = NULL;
  uint32_t                        size;
  
  /* allocate service */
  size  = sizeof(__IOHIDKeyboardEventTranslator) - sizeof(CFRuntimeBase);
  translator = (IOHIDKeyboardEventTranslatorRef)_CFRuntimeCreateInstance(allocator, IOHIDKeyboardEventTranslatorGetTypeID(), size, NULL);
  
  if (!translator)
    return NULL;
  
  offset = translator;
  bzero((uint8_t*)offset + sizeof(CFRuntimeBase), size);
  
  return translator;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorInit
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void __IOHIDKeyboardEventTranslatorInit( IOHIDKeyboardEventTranslatorRef translator , void * service, FServiceCopyProperty propertyFunc)
{
  CFNumberRef keyboardID = NULL;
  if (service) {
    keyboardID = propertyFunc (service, CFSTR(kIOHIDSubinterfaceIDKey));
    if (keyboardID) {
      CFNumberGetValue(keyboardID, kCFNumberSInt32Type, &translator->keyboardID);
      CFRelease(keyboardID);
    }
  }
  translator->isISO = __IOHIDKeyboardEventTranslatorIsISOKeyboard(translator->keyboardID);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorFree
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void __IOHIDKeyboardEventTranslatorFree( CFTypeRef object )
{
  (void)object;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorCopyDebugDescription
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static CFStringRef __IOHIDKeyboardEventTranslatorCopyDebugDescription(CFTypeRef cf)
{
  IOHIDKeyboardEventTranslatorRef translator = ( IOHIDKeyboardEventTranslatorRef ) cf;
  
  return CFStringCreateWithFormat(CFGetAllocator(cf), NULL, CFSTR("IOHIDKeyboardEventTranslator: keyboardID %x"), translator->keyboardID);
}

#define USAGE_INDEX(i) (i-kHIDUsage_KeyboardRightArrow)

static uint32_t AdditionModifierMask [] = {
  [USAGE_INDEX(kHIDUsage_KeyboardRightArrow)]      = NX_NUMERICPADMASK,    /* Right Arrow */
  [USAGE_INDEX(kHIDUsage_KeyboardLeftArrow)]       = NX_NUMERICPADMASK,    /* Left Arrow */
  [USAGE_INDEX(kHIDUsage_KeyboardDownArrow)]       = NX_NUMERICPADMASK,    /* Down Arrow */
  [USAGE_INDEX(kHIDUsage_KeyboardUpArrow)]         = NX_NUMERICPADMASK,    /* Up Arrow */
  [USAGE_INDEX(kHIDUsage_KeypadNumLock)]           = NX_NUMERICPADMASK,    /* Keypad NumLock or Clear */
  [USAGE_INDEX(kHIDUsage_KeypadSlash)]             = NX_NUMERICPADMASK,    /* Keypad / */
  [USAGE_INDEX(kHIDUsage_KeypadAsterisk)]          = NX_NUMERICPADMASK,    /* Keypad * */
  [USAGE_INDEX(kHIDUsage_KeypadHyphen)]            = NX_NUMERICPADMASK,    /* Keypad - */
  [USAGE_INDEX(kHIDUsage_KeypadPlus)]              = NX_NUMERICPADMASK,    /* Keypad + */
  [USAGE_INDEX(kHIDUsage_KeypadEnter)]             = NX_NUMERICPADMASK,    /* Keypad Enter */
  [USAGE_INDEX(kHIDUsage_Keypad1)]                 = NX_NUMERICPADMASK,    /* Keypad 1 or End */
  [USAGE_INDEX(kHIDUsage_Keypad2)]                 = NX_NUMERICPADMASK,    /* Keypad 2 or Down Arrow */
  [USAGE_INDEX(kHIDUsage_Keypad3)]                 = NX_NUMERICPADMASK,    /* Keypad 3 or Page Down */
  [USAGE_INDEX(kHIDUsage_Keypad4)]                 = NX_NUMERICPADMASK,    /* Keypad 4 or Left Arrow */
  [USAGE_INDEX(kHIDUsage_Keypad5)]                 = NX_NUMERICPADMASK,    /* Keypad 5 */
  [USAGE_INDEX(kHIDUsage_Keypad6)]                 = NX_NUMERICPADMASK,    /* Keypad 6 or Right Arrow */
  [USAGE_INDEX(kHIDUsage_Keypad7)]                 = NX_NUMERICPADMASK,    /* Keypad 7 or Home */
  [USAGE_INDEX(kHIDUsage_Keypad8)]                 = NX_NUMERICPADMASK,    /* Keypad 8 or Up Arrow */
  [USAGE_INDEX(kHIDUsage_Keypad9)]                 = NX_NUMERICPADMASK,    /* Keypad 9 or Page Up */
  [USAGE_INDEX(kHIDUsage_Keypad0)]                 = NX_NUMERICPADMASK,    /* Keypad 0 or Insert */
  [USAGE_INDEX(kHIDUsage_KeypadPeriod)]            = NX_NUMERICPADMASK     /* Keypad . or Delete */
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorGetNonModifierFlags
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static uint32_t __IOHIDKeyboardEventTranslatorGetNonModifierFlags (uint32_t  usage, uint32_t  usagePage) {

  uint32_t flags = NX_NONCOALSESCEDMASK;
  if (usagePage == kHIDPage_KeyboardOrKeypad && usage >= kHIDUsage_KeyboardRightArrow && USAGE_INDEX(usage) < sizeof(AdditionModifierMask) / sizeof(AdditionModifierMask[0])) {
    flags |= AdditionModifierMask[USAGE_INDEX(usage)];
  }
  return flags;
}

const MODIFIER_INFO ModifierInfoTable [] = {
    {
        kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftControl,
        NX_CONTROLMASK | NX_DEVICELCTLKEYMASK,
        NX_SUBTYPE_STICKYKEYS_CONTROL_UP,
        NX_SUBTYPE_STICKYKEYS_CONTROL_DOWN,
        NX_SUBTYPE_STICKYKEYS_CONTROL_LOCK
    },
    {
        kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightControl,
        NX_CONTROLMASK | NX_DEVICERCTLKEYMASK,
        NX_SUBTYPE_STICKYKEYS_CONTROL_UP,
        NX_SUBTYPE_STICKYKEYS_CONTROL_DOWN,
        NX_SUBTYPE_STICKYKEYS_CONTROL_LOCK
    },
    {
        kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftShift,
        NX_SHIFTMASK | NX_DEVICELSHIFTKEYMASK,
        NX_SUBTYPE_STICKYKEYS_SHIFT_UP,
        NX_SUBTYPE_STICKYKEYS_SHIFT_DOWN,
        NX_SUBTYPE_STICKYKEYS_SHIFT_LOCK,
    },
    {
        kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightShift,
        NX_SHIFTMASK | NX_DEVICERSHIFTKEYMASK,
        NX_SUBTYPE_STICKYKEYS_SHIFT_UP,
        NX_SUBTYPE_STICKYKEYS_SHIFT_DOWN,
        NX_SUBTYPE_STICKYKEYS_SHIFT_LOCK,
    },
    {
        kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftAlt,
        NX_ALTERNATEMASK | NX_DEVICELALTKEYMASK,
        NX_SUBTYPE_STICKYKEYS_ALTERNATE_UP,
        NX_SUBTYPE_STICKYKEYS_ALTERNATE_DOWN,
        NX_SUBTYPE_STICKYKEYS_ALTERNATE_LOCK
    },
    {
        kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightAlt,
        NX_ALTERNATEMASK | NX_DEVICERALTKEYMASK,
        NX_SUBTYPE_STICKYKEYS_ALTERNATE_UP,
        NX_SUBTYPE_STICKYKEYS_ALTERNATE_DOWN,
        NX_SUBTYPE_STICKYKEYS_ALTERNATE_LOCK
    },
    {
        kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftGUI,
         NX_COMMANDMASK | NX_DEVICELCMDKEYMASK,
         NX_SUBTYPE_STICKYKEYS_COMMAND_UP,
         NX_SUBTYPE_STICKYKEYS_COMMAND_DOWN,
         NX_SUBTYPE_STICKYKEYS_COMMAND_LOCK
    },
    {
        kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightGUI,
        NX_COMMANDMASK | NX_DEVICERCMDKEYMASK,
        NX_SUBTYPE_STICKYKEYS_COMMAND_UP,
        NX_SUBTYPE_STICKYKEYS_COMMAND_DOWN,
        NX_SUBTYPE_STICKYKEYS_COMMAND_LOCK
    },
    {
        kHIDPage_AppleVendorTopCase, kHIDUsage_AV_TopCase_KeyboardFn,
        NX_SECONDARYFNMASK,
        NX_SUBTYPE_STICKYKEYS_FN_UP,
        NX_SUBTYPE_STICKYKEYS_FN_DOWN,
        NX_SUBTYPE_STICKYKEYS_FN_LOCK,
    },
    {
        kHIDPage_AppleVendorKeyboard, kHIDUsage_AppleVendorKeyboard_Function,
        NX_SECONDARYFNMASK,
        NX_SUBTYPE_STICKYKEYS_FN_UP,
        NX_SUBTYPE_STICKYKEYS_FN_DOWN,
        NX_SUBTYPE_STICKYKEYS_FN_LOCK,
    }
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorGetModifierInfo
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static const MODIFIER_INFO* __IOHIDKeyboardEventTranslatorGetModifierInfo (uint32_t  usage, uint32_t  usagePage) {

  for (size_t index = 0; index < sizeof(ModifierInfoTable) / sizeof(ModifierInfoTable[0]); index++) {
    if (ModifierInfoTable[index].usagePage == usagePage && ModifierInfoTable[index].usage == usage) {
        return &ModifierInfoTable[index];
    }
  }
  return NULL;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorGetKeyboardKeyCode
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

static uint8_t __IOHIDKeyboardEventTranslatorGetKeyboardKeyCode (uint32_t  usage, uint32_t  usagePage, boolean_t isISO) {
  
  uint8_t keyCode = NX_UNKNOWNKEY;
  
  switch (usagePage) {
    case kHIDPage_KeyboardOrKeypad:
      if (usage < hid_usb_2_adb_keymap_length / sizeof (hid_usb_2_adb_keymap[0])) {
        keyCode = hid_usb_2_adb_keymap[usage];
      }
      if (isISO) {
        switch (usage) {
          case kHIDUsage_KeyboardNonUSBackslash:
            keyCode = 50;
            break;
          case kHIDUsage_KeyboardGraveAccentAndTilde:
            keyCode = 10;
            break;
        }
      }
      break;
    case kHIDPage_AppleVendorKeyboard:
      if (usage < hid_usb_apple_2_adb_keymap_length / sizeof (hid_usb_apple_2_adb_keymap[0])) {
        keyCode = hid_usb_apple_2_adb_keymap[usage];
      }
      if (usage == kHIDUsage_AppleVendorKeyboard_Function) {
        keyCode = 0x3f;
      }
      break;
    case kHIDPage_AppleVendorTopCase:
      if (usage == kHIDUsage_AV_TopCase_KeyboardFn) {
        keyCode = 0x3f;
      }
      break;
    case kHIDPage_Consumer:
      switch (usage) {
        case kHIDUsage_Csmr_ACDesktopShowAllWindows:
          keyCode = 0xa0;
          break;
        case kHIDUsage_Csmr_ACDesktopShowAllApplications:
          keyCode = 0x83;
          break;
      }
      break;
  }
  return keyCode;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorGetConsumerKeyCode
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

static uint8_t __IOHIDKeyboardEventTranslatorGetConsumerKeyCode (uint32_t  usage, uint32_t  usagePage) {
  
  uint8_t keyCode = NX_UNKNOWNKEY;
  
  if (usagePage == kHIDPage_Consumer) {
    switch(usage) {
      case kHIDUsage_Csmr_Power:
      case kHIDUsage_Csmr_Reset:
      case kHIDUsage_Csmr_Sleep:
        keyCode = NX_POWER_KEY;
        break;
      case kHIDUsage_Csmr_Play:
      case kHIDUsage_Csmr_PlayOrPause:
      case kHIDUsage_Csmr_PlayOrSkip:
        keyCode = NX_KEYTYPE_PLAY;
        break;
      case kHIDUsage_Csmr_ScanNextTrack:
        keyCode = NX_KEYTYPE_NEXT;
        break;
      case kHIDUsage_Csmr_ScanPreviousTrack:
        keyCode = NX_KEYTYPE_PREVIOUS;
        break;
      case kHIDUsage_Csmr_FastForward:
        keyCode = NX_KEYTYPE_FAST;
        break;
      case kHIDUsage_Csmr_Rewind:
        keyCode = NX_KEYTYPE_REWIND;
        break;
      case kHIDUsage_Csmr_StopOrEject:
      case kHIDUsage_Csmr_Eject:
        keyCode = NX_KEYTYPE_EJECT;
        break;
      case kHIDUsage_Csmr_VolumeIncrement:
        keyCode = NX_KEYTYPE_SOUND_UP;
        break;
      case kHIDUsage_Csmr_VolumeDecrement:
        keyCode = NX_KEYTYPE_SOUND_DOWN;
        break;
      case kHIDUsage_Csmr_Mute:
        keyCode = NX_KEYTYPE_MUTE;
        break;
      case kHIDUsage_Csmr_DisplayBrightnessIncrement:
        keyCode = NX_KEYTYPE_BRIGHTNESS_UP;
        break;
      case kHIDUsage_Csmr_DisplayBrightnessDecrement:
        keyCode = NX_KEYTYPE_BRIGHTNESS_DOWN;
        break;
      case kHIDUsage_Csmr_Menu:
        keyCode = NX_KEYTYPE_MENU;
        break;
      default:
        break;
    }
  } else if (usagePage == kHIDPage_GenericDesktop)
  {
    switch (usage) {
      case kHIDUsage_GD_SystemPowerDown:
      case kHIDUsage_GD_SystemSleep:
      case kHIDUsage_GD_SystemWakeUp:
        keyCode = NX_POWER_KEY;
        break;
    }
  } else if (usagePage == kHIDPage_AppleVendorTopCase) {
    switch (usage) {
      case kHIDUsage_AV_TopCase_BrightnessUp:
        keyCode = NX_KEYTYPE_BRIGHTNESS_UP;
        break;
      case kHIDUsage_AV_TopCase_BrightnessDown:
        keyCode = NX_KEYTYPE_BRIGHTNESS_DOWN;
        break;
      case kHIDUsage_AV_TopCase_VideoMirror:
        keyCode = NX_KEYTYPE_VIDMIRROR;
        break;
      case kHIDUsage_AV_TopCase_IlluminationDown:
        keyCode = NX_KEYTYPE_ILLUMINATION_DOWN;
        break;
      case kHIDUsage_AV_TopCase_IlluminationUp:
        keyCode = NX_KEYTYPE_ILLUMINATION_UP;
        break;
      case kHIDUsage_AV_TopCase_IlluminationToggle:
        keyCode = NX_KEYTYPE_ILLUMINATION_TOGGLE;
        break;
    }
  } else if (usagePage == kHIDPage_KeyboardOrKeypad) {
    switch (usage) {
      case kHIDUsage_KeyboardLockingNumLock:
      case kHIDUsage_KeypadNumLock:
        keyCode = NX_KEYTYPE_NUM_LOCK;
        break;
      case kHIDUsage_KeyboardCapsLock:
        keyCode = NX_KEYTYPE_CAPS_LOCK;
        break;
      case kHIDUsage_KeyboardPower:
        keyCode = NX_POWER_KEY;
        break;
      case kHIDUsage_KeyboardMute:
        keyCode = NX_KEYTYPE_MUTE;
        break;
      case kHIDUsage_KeyboardVolumeUp:
        keyCode = NX_KEYTYPE_SOUND_UP;
        break;
      case kHIDUsage_KeyboardVolumeDown:
        keyCode = NX_KEYTYPE_SOUND_DOWN;
        break;
    }
  }
  return keyCode;
}



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorIsISSOKeyboard
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static uint16_t __IOHIDKeyboardEventTranslatorIsISOKeyboard (uint32_t keyboardID) {
  switch (keyboardID) {
    case kgestUSBCosmoISOKbd:
    case kgestUSBAndyISOKbd:
    case kgestQ6ISOKbd:
    case kgestQ30ISOKbd:
    case kgestM89ISOKbd:
    case kgestUSBGenericISOkd:
      return true;
  }
  return false;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorInitNxEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorInitNxEvent (EVENT_TRANSLATOR_CONTEXT *context, NXEventExt *nxEvent, uint8_t type) {
  memset(nxEvent, 0, sizeof(*nxEvent));
  nxEvent->payload.service_id   = context->serviceid;
  nxEvent->payload.time         = context->timestamp;
  nxEvent->payload.type         = type;
  nxEvent->extension.flags      = NX_EVENT_EXTENSION_LOCATION_INVALID;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorCreateHidEventForNXEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define __IOHIDKeyboardEventTranslatorCreateHidEventForNXEvent(nxEvent, hidEvent) \
  IOHIDEventCreateVendorDefinedEvent (          \
      CFGetAllocator(hidEvent),                 \
      nxEvent.payload.time,                     \
      kHIDPage_AppleVendor,                     \
      kHIDUsage_AppleVendor_NXEvent_Translated, \
      0,                                        \
      (uint8_t*)&nxEvent,                       \
      sizeof (nxEvent),                         \
      0                                         \
      )



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorAddStickyKeyEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorAddStickyKeyEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context)
{
    uint16_t    subType = 0;
    
    if (context->modifier) {
        switch (IOHIDEventGetIntegerValue(context->event, kIOHIDEventFieldKeyboardStickyKeyPhase)) {
            case kIOHIDKeyboardStickyKeyPhaseUp:
                subType = context->modifier->stickyUpType;
                break;
            case kIOHIDKeyboardStickyKeyPhaseDown:
                subType = context->modifier->stickyDownType;
                break;
            case kIOHIDKeyboardStickyKeyPhaseLocked:
                subType = context->modifier->stickyLockType;
                break;
            default:
                break;
        }
    }
    
    if (subType) {
        __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (translator, context, subType);
        return;
    }
    
    switch (IOHIDEventGetIntegerValue(context->event, kIOHIDEventFieldKeyboardStickyKeyToggle)) {
        case kIOHIDKeyboardStickyKeyToggleOn:
            subType = NX_SUBTYPE_STICKYKEYS_ON;
            break;
        case kIOHIDKeyboardStickyKeyToggleOff:
            subType = NX_SUBTYPE_STICKYKEYS_OFF;
            break;
        default:
            break;
    }
    
    if (subType) {
        __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (translator, context, subType);
    }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorAppendAuxButtonEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorAddAuxButtonEvent (IOHIDKeyboardEventTranslatorRef translator __unused, EVENT_TRANSLATOR_CONTEXT *context, uint8_t keyCode)
{
  IOHIDEventRef translatedEvent;
  CFIndex       slowKeyPhase = IOHIDEventGetIntegerValue (context->event, kIOHIDEventFieldKeyboardSlowKeyPhase);
  
  NXEventExt nxEvent;
  
  __IOHIDKeyboardEventTranslatorInitNxEvent (context, &nxEvent, NX_SYSDEFINED);
  
  nxEvent.payload.flags = context->eventFlags | __IOHIDKeyboardEventTranslatorGetNonModifierFlags (context->usage, context->usagePage);


  nxEvent.payload.data.compound.subType = NX_SUBTYPE_AUX_CONTROL_BUTTONS;

  nxEvent.payload.data.compound.misc.L[0] = (keyCode << 16) |
                                            ((context->down ? NX_KEYDOWN : NX_KEYUP) << 8) |
                                            ((context->flags & kIOHIDKeyboardIsRepeat && slowKeyPhase != kIOHIDKeyboardSlowKeyOn) ? 1 : 0);
  
  nxEvent.payload.data.compound.misc.L[1] = 0xffffffff;
  nxEvent.payload.data.compound.misc.L[2] = 0xffffffff;
  
  translatedEvent = __IOHIDKeyboardEventTranslatorCreateHidEventForNXEvent(nxEvent, context->event);
  if (translatedEvent) {
    CFArrayAppendValue(context->collection, translatedEvent);
    CFRelease (translatedEvent);
  }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorAddMenuButtonEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorAddMenuButtonEvent (IOHIDKeyboardEventTranslatorRef translator , EVENT_TRANSLATOR_CONTEXT *context)
{
    uint8_t subType     = 0;
    CFIndex clickCount  = IOHIDEventGetIntegerValue(context->event, kIOHIDEventFieldKeyboardPressCount);

    if (clickCount == 3 && context->down && (IOHIDEventGetPhase(context->event) & kIOHIDEventPhaseEnded) == 0) {
        subType = NX_SUBTYPE_ACCESSIBILITY;
    } else if (IOHIDEventGetPhase(context->event) & kIOHIDEventPhaseEnded && IOHIDEventGetIntegerValue(context->event, kIOHIDEventFieldKeyboardPressCount) == 1) {
        subType = NX_SUBTYPE_MENU;
    }
    if (subType) {
        __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (translator, context, subType);
    }
    
    return;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorAddConsumerEvents
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorAddConsumerEvents (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint8_t keyCode)
{
  uint8_t subType = 0;
  
  switch (keyCode) {
  case NX_KEYTYPE_MENU:
    __IOHIDKeyboardEventTranslatorAddMenuButtonEvent (translator, context);
    return;
  case NX_POWER_KEY:
  case NX_KEYTYPE_EJECT:
    if (( context->eventFlags & kNXFlagsModifierMask) == (NX_ALTERNATEMASK | NX_COMMANDMASK)) {
      subType = NX_SUBTYPE_SLEEP_EVENT;
    } else if (( context->eventFlags & kNXFlagsModifierMask) == (NX_COMMANDMASK | NX_CONTROLMASK)) {
      subType = NX_SUBTYPE_RESTART_EVENT;
    } else if (( context->eventFlags & kNXFlagsModifierMask) == (NX_COMMANDMASK | NX_CONTROLMASK | NX_ALTERNATEMASK)) {
      subType = NX_SUBTYPE_SHUTDOWN_EVENT;
    } else if (( context->eventFlags & kNXFlagsModifierMask) == NX_CONTROLMASK) {
      subType = NX_SUBTYPE_POWER_KEY;
    } else{
      subType = (keyCode == NX_KEYTYPE_EJECT) ? NX_SUBTYPE_EJECT_KEY : NX_SUBTYPE_POWER_KEY;
      
      if (keyCode == NX_POWER_KEY) {
        __IOHIDKeyboardEventTranslatorAddKeyboardEvent(translator, context, ADB_POWER_KEY_CODE);
      }
    }
    break;
    default:
      // Don't create NXEvent for synthetic consumer keyboard multi-click IOHIDEvents (PhaseEnded events and LongPress events).
      if (IOHIDEventGetPhase(context->event) & kIOHIDEventPhaseEnded ||
          IOHIDEventGetIntegerValue(context->event, kIOHIDEventFieldKeyboardLongPress) != 0) {
        return;
      }
      break;
  }
  
  if (subType && context->down) {
    __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (translator, context, subType);
  }
  if (subType == 0 || subType == NX_SUBTYPE_EJECT_KEY || subType == NX_SUBTYPE_POWER_KEY) {
    __IOHIDKeyboardEventTranslatorAddAuxButtonEvent (translator, context, keyCode);
  }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorAddFlagsChangeEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorAddFlagsChangeEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint32_t flags)
{
  IOHIDEventRef translatedEvent;
  NXEventExt    nxEvent;
  
  __IOHIDKeyboardEventTranslatorInitNxEvent (context, &nxEvent, NX_FLAGSCHANGED);
  
  nxEvent.payload.flags = flags | __IOHIDKeyboardEventTranslatorGetNonModifierFlags (context->usage, context->usagePage);
  nxEvent.payload.data.key.keyCode = __IOHIDKeyboardEventTranslatorGetKeyboardKeyCode (context->usage, context->usagePage, translator->isISO);
  nxEvent.payload.data.key.keyboardType = translator->keyboardID;
  
  translatedEvent = __IOHIDKeyboardEventTranslatorCreateHidEventForNXEvent(nxEvent, context->event);
  if (translatedEvent) {
    CFArrayAppendValue(context->collection, translatedEvent);
    CFRelease (translatedEvent);
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorAddKeyboardEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorAddKeyboardEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint8_t keyCode)
{
  IOHIDEventRef translatedEvent;
  NXEventExt    nxEvent;
  
  __IOHIDKeyboardEventTranslatorInitNxEvent (context, &nxEvent, context->down ? NX_KEYDOWN : NX_KEYUP);
 
  nxEvent.payload.flags = translator->eventFlags | __IOHIDKeyboardEventTranslatorGetNonModifierFlags (context->usage, context->usagePage);
  nxEvent.payload.data.key.keyCode = keyCode;
  nxEvent.payload.data.key.keyboardType = translator->keyboardID;
  nxEvent.payload.data.key.repeat = (context->flags & kIOHIDKeyboardIsRepeat) ? 1 : 0;

  translatedEvent = __IOHIDKeyboardEventTranslatorCreateHidEventForNXEvent(nxEvent, context->event);
  if (translatedEvent) {
    CFArrayAppendValue(context->collection, translatedEvent);
    CFRelease (translatedEvent);
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventProcessModifiers
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventProcessModifiers (IOHIDKeyboardEventTranslatorRef translator __unused, EVENT_TRANSLATOR_CONTEXT *context) {
  
  
  if (context->translationFlags & kTranslationFlagCapsLockOn) {
    context->eventFlags |= NX_ALPHASHIFTMASK;
  } else {
    context->eventFlags &= ~NX_ALPHASHIFTMASK;
  }
  
  if (context->modifier != NULL) {
    if (context->down) {
      context->eventFlags |= context->modifier->modifierMask;
    } else {
      context->eventFlags &= ~(context->modifier->modifierMask);
    }
  }
  if (context->eventFlags & (NX_DEVICELSHIFTKEYMASK | NX_DEVICERSHIFTKEYMASK)) {
    context->eventFlags |= NX_SHIFTMASK;
  }
  if (context->eventFlags & (NX_DEVICELCTLKEYMASK | NX_DEVICERCTLKEYMASK)) {
    context->eventFlags |= NX_CONTROLMASK;
  }
  if (context->eventFlags & (NX_DEVICELCMDKEYMASK | NX_DEVICERCMDKEYMASK)) {
    context->eventFlags |= NX_COMMANDMASK;
  }
  if (context->eventFlags & (NX_DEVICELALTKEYMASK | NX_DEVICERALTKEYMASK)) {
    context->eventFlags |= NX_ALTERNATEMASK;
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint16_t subtype)
{
    IOHIDEventRef   translatedEvent;
    NXEventExt      nxEvent;
  
   __IOHIDKeyboardEventTranslatorInitNxEvent (context, &nxEvent, NX_SYSDEFINED);
  
    nxEvent.payload.flags = translator->eventFlags;
    nxEvent.payload.data.compound.subType = subtype;
  
    translatedEvent = __IOHIDKeyboardEventTranslatorCreateHidEventForNXEvent(nxEvent, context->event);
    if (translatedEvent) {
        CFArrayAppendValue(context->collection, translatedEvent);
        CFRelease (translatedEvent);
    }

}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorAddSlowKeyPhaseEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDKeyboardEventTranslatorAddSlowKeyPhaseEvent (IOHIDKeyboardEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context)
{
    CFIndex     slowKeyPhase = IOHIDEventGetIntegerValue (context->event, kIOHIDEventFieldKeyboardSlowKeyPhase);
    uint16_t    subType;

    if (slowKeyPhase == kIOHIDKeyboardSlowKeyPhaseStart) {
        subType = NX_SUBTYPE_SLOWKEYS_START;
    } else if (slowKeyPhase == kIOHIDKeyboardSlowKeyPhaseAbort) {
        subType =  NX_SUBTYPE_SLOWKEYS_ABORT;
    } else {
        return;
    }
    
    __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (translator, context, subType);

}
//------------------------------------------------------------------------------
// IOHIDKeyboardEventTranslatorCreateEventCollection
//------------------------------------------------------------------------------

CFArrayRef IOHIDKeyboardEventTranslatorCreateEventCollection(IOHIDKeyboardEventTranslatorRef translator, IOHIDEventRef keyboardEvent, uint32_t flags) {
  
  uint8_t keyCode;
  
  if (keyboardEvent == NULL || translator == NULL) {
    return NULL;
  }
  
  IOHIDEventRef kbcEvent = IOHIDEventGetEvent (keyboardEvent, kIOHIDEventTypeKeyboard);
  if (kbcEvent == NULL) {
    return NULL;
  }
    
  CFMutableArrayRef eventCollection = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
  if (eventCollection == NULL) {
    return NULL;
  }
  
  EVENT_TRANSLATOR_CONTEXT context;
  
  context.usage       = (uint32_t)IOHIDEventGetIntegerValue (kbcEvent, kIOHIDEventFieldKeyboardUsage);
  context.flags       = (uint32_t)IOHIDEventGetEventFlags (kbcEvent);
  context.usagePage   = (uint32_t)IOHIDEventGetIntegerValue (kbcEvent, kIOHIDEventFieldKeyboardUsagePage);
  context.down        = (boolean_t)IOHIDEventGetIntegerValue (kbcEvent, kIOHIDEventFieldKeyboardDown);
  context.timestamp   = IOHIDEventGetTimeStamp (kbcEvent);
  context.serviceid   = IOHIDEventGetSenderID(kbcEvent);
  context.modifier    = __IOHIDKeyboardEventTranslatorGetModifierInfo (context.usage, context.usagePage);
  context.translationFlags = flags;
  context.eventFlags  = translator->eventFlags;
  context.skipTranslationPhase = 0;
  context.collection  = eventCollection;
  context.event       = kbcEvent;
  
  CFIndex slowKeyPhase = IOHIDEventGetIntegerValue (kbcEvent, kIOHIDEventFieldKeyboardSlowKeyPhase);
  
  __IOHIDKeyboardEventProcessModifiers (translator, &context);
  
  do {
    if (IOHIDEventGetIntegerValue(kbcEvent, kIOHIDEventFieldKeyboardStickyKeyPhase) || IOHIDEventGetIntegerValue(kbcEvent, kIOHIDEventFieldKeyboardStickyKeyToggle)) {
      __IOHIDKeyboardEventTranslatorAddStickyKeyEvent (translator, &context);
    } else if (slowKeyPhase == kIOHIDKeyboardSlowKeyPhaseStart || slowKeyPhase == kIOHIDKeyboardSlowKeyPhaseAbort) {
      __IOHIDKeyboardEventTranslatorAddSlowKeyPhaseEvent (translator, &context);
      continue;
    }
    
    
    keyCode = __IOHIDKeyboardEventTranslatorGetConsumerKeyCode (context.usage, context.usagePage);
    if (keyCode != NX_UNKNOWNKEY) {
      __IOHIDKeyboardEventTranslatorAddConsumerEvents (translator, &context, keyCode);
      //continue;
    }

    if (context.eventFlags != translator->eventFlags) {
      translator->eventFlags = context.eventFlags;
      __IOHIDKeyboardEventTranslatorAddFlagsChangeEvent(translator, &context, translator->eventFlags);
    }
  
    if (!context.modifier && (keyCode == NX_UNKNOWNKEY || keyCode == NX_KEYTYPE_NUM_LOCK)) {
      keyCode = __IOHIDKeyboardEventTranslatorGetKeyboardKeyCode (context.usage, context.usagePage, translator->isISO);
      if (keyCode != NX_UNKNOWNKEY && keyCode != ADB_CAPSLOCK_KEY_CODE) {
        __IOHIDKeyboardEventTranslatorAddKeyboardEvent(translator, &context, keyCode);
      }
    }

  } while (0);

  if (IOHIDEventGetIntegerValue(kbcEvent, kIOHIDEventFieldKeyboardMouseKeyToggle)) {
    __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (translator, &context, NX_SUBTYPE_STICKYKEYS_TOGGLEMOUSEDRIVING);
  } else if (slowKeyPhase == kIOHIDKeyboardSlowKeyOn && context.down) {
    __IOHIDKeyboardEventTranslatorAddSysdefinedEventWithSubtype (translator, &context, NX_SUBTYPE_SLOWKEYS_END);
  }
  
  return eventCollection;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardEventTranslatorGetModifierFlags
//------------------------------------------------------------------------------
uint32_t IOHIDKeyboardEventTranslatorGetModifierFlags(IOHIDKeyboardEventTranslatorRef translator) {
  if (translator) {
    return translator->eventFlags;
  }
  return 0;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardEventTranslatorUpdateWithCompanionModifiers
//------------------------------------------------------------------------------
void IOHIDKeyboardEventTranslatorUpdateWithCompanionModifiers (IOHIDEventRef nxEvent, uint32_t modifierFlags) {
  if (nxEvent == NULL || modifierFlags == 0) {
    return;
  }
  NXEventExt *event= NULL;
  CFIndex eventLength = 0;
  IOHIDEventGetVendorDefinedData (nxEvent, (uint8_t**)&event, &eventLength);
  event->payload.flags |= modifierFlags;
  if (event->payload.type == NX_SYSDEFINED && (event->payload.data.compound.subType == NX_SUBTYPE_EJECT_KEY || event->payload.data.compound.subType == NX_SUBTYPE_POWER_KEY)) {
    if ((event->payload.flags & kNXFlagsModifierMask) == (NX_ALTERNATEMASK | NX_COMMANDMASK)) {
      event->payload.data.compound.subType = NX_SUBTYPE_SLEEP_EVENT;
    } else if ((event->payload.flags & kNXFlagsModifierMask) == (NX_COMMANDMASK | NX_CONTROLMASK)) {
      event->payload.data.compound.subType = NX_SUBTYPE_RESTART_EVENT;
    } else if ((event->payload.flags & kNXFlagsModifierMask) == (NX_COMMANDMASK | NX_CONTROLMASK | NX_ALTERNATEMASK)) {
      event->payload.data.compound.subType = NX_SUBTYPE_SHUTDOWN_EVENT;
    } else if ((event->payload.flags & kNXFlagsModifierMask) == NX_CONTROLMASK) {
      event->payload.data.compound.subType = NX_SUBTYPE_POWER_KEY;
    }
  }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardEventTranslatorSetProperty
//------------------------------------------------------------------------------
void IOHIDKeyboardEventTranslatorSetProperty (IOHIDKeyboardEventTranslatorRef translator, CFStringRef key, CFTypeRef property) {
  if (key && property && CFEqual(key, CFSTR(kIOHIDSubinterfaceIDKey)) && CFGetTypeID(property) == CFNumberGetTypeID()) {
    CFNumberGetValue(property, kCFNumberSInt32Type, &translator->keyboardID);
    translator->isISO = __IOHIDKeyboardEventTranslatorIsISOKeyboard(translator->keyboardID);
  }
}
