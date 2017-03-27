//
//  IOHIDPointerEventTranslation.h
//  IOHIDFamily
//
//  Created by yg on 12/23/15.
//
//

#ifndef   IOHIDPointerEventTranslation_h
#define   IOHIDPointerEventTranslation_h

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __IOHIDPointerEventTranslator * IOHIDPointerEventTranslatorRef;

CFTypeID IOHIDPointerEventTranslatorGetTypeID(void);
IOHIDPointerEventTranslatorRef IOHIDPointerEventTranslatorCreate (CFAllocatorRef allocator, uint32_t options);
void IOHIDPointerEventTranslatorRegisterService (IOHIDPointerEventTranslatorRef translator, CFTypeRef service);
void IOHIDPointerEventTranslatorUnRegisterService (IOHIDPointerEventTranslatorRef translator, CFTypeRef service);
CFArrayRef IOHIDPointerEventTranslatorCreateEventCollection (IOHIDPointerEventTranslatorRef translator, IOHIDEventRef event, CFTypeRef sender, uint32_t flags, uint32_t options);
  
void IOHIDPointerEventTranslatorSetProperty (IOHIDPointerEventTranslatorRef translator, CFStringRef key, CFTypeRef property) ;
uint32_t IOHIDPointerEventTranslatorGetGlobalButtonState (IOHIDPointerEventTranslatorRef translator);
#ifdef __cplusplus
}
#endif

#endif /*IOHIDPointerEventTranslation_h */
