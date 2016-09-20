//
//  IOHIDKeyboardEventTranslation.h
//  IOHIDFamily
//
//  Created by yg on 12/4/15.
//
//

#ifndef IOHIDKeyboardEventTranslation_h
#define IOHIDKeyboardEventTranslation_h

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDServiceClient.h>
#include <IOKit/hid/IOHIDService.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __IOHIDKeyboardEventTranslator * IOHIDKeyboardEventTranslatorRef;

enum {
  kTranslationFlagCapsLockOn = 0x1
};
  
CFTypeID IOHIDKeyboardEventTranslatorGetTypeID(void);

IOHIDKeyboardEventTranslatorRef IOHIDKeyboardEventTranslatorCreateWithService(CFAllocatorRef allocator, IOHIDServiceRef service);

IOHIDKeyboardEventTranslatorRef IOHIDKeyboardEventTranslatorCreateWithServiceClient(CFAllocatorRef allocator, IOHIDServiceClientRef service);

CFArrayRef IOHIDKeyboardEventTranslatorCreateEventCollection(IOHIDKeyboardEventTranslatorRef translator, IOHIDEventRef keyboardEvent, uint32_t flags);

uint32_t IOHIDKeyboardEventTranslatorGetModifierFlags(IOHIDKeyboardEventTranslatorRef translator);
  
void IOHIDKeyboardEventTranslatorUpdateWithCompanionModifiers (IOHIDEventRef nxEvent, uint32_t modifierFlags);

void IOHIDKeyboardEventTranslatorSetProperty (IOHIDKeyboardEventTranslatorRef translator, CFStringRef key, CFTypeRef property);
  
#ifdef __cplusplus
}
#endif

#endif /* IOHIDKeyboardEventTranslation_h */
