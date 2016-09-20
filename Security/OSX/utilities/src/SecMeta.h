/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef _UTILITIES_SECMETA_H_
#define _UTILITIES_SECMETA_H_

#include <CoreFoundation/CoreFoundation.h>

//
// MARK - SecMeta
// MARK - Logging, Trace, Error reporting, action log capture, and more.
//

#define SecInline static inline

// Disable all logging.
#define SecDisableLogging() do { _secOptions = _SecClearMask(_secOptions, mask); } while(0)

// For people who don't like flag1|flag2|flag3 syntax use SecFlags(flag1,flag2,flag3)
#define SecFlags(...)  _SecFlags(0, __VA_ARGS__)

// Set the current scopes log level.
#define SecSetLogLevel(level)  _SecSetLogLevel(&secOptions, (level));

// Mark that we performed an action for the log and for an generated errors.
#define SecAction(flags, key, action,...) \
    if (_SecLogLevel(flags)) { _SecSetLogLevel(&_secFlags, _SecLogLevel(flags)); } \
    if (_SecSetFlags(flags) \
    if ((_secFlags | flags) & kSecTraceAction) {} \
    if ((_secFlags | flags) & kSecLogEveryActionFlag) { \
        if (flags & kSecLogLevelMask) { \


        } \
    } \
//    _SecAction(&_secResult, &_secFlags, &_secName, &_secError, &_secChain, &_secActions, flags, key, action, __VA_ARGS__)

// Usage: declare a variable that represents the result of your function
#define SecTry(result,flags,humanReadableFunctionNameForLogs) switch (0) case 0: { \
    __typeof__(result) _secResult = result; \
    __typeof__(flags) _secFlags = flags; \
    __typeof__(format) _secName = humanReadableFunctionNameForLogs; \
    CFErrorRef _secError = NULL; \
    CFMutableArrayRef _secChain = NULL; \
    CFMutableStringRef _secActions = NULL; \
    CFMutableDictionaryRef _secFormatOptions = NULL; \
    SecAction(_secFlags, CFSTR("entered"));



#define SecEnableFlags(&_secFlags, flags) _secFlags = (flags);
#define SecDisableFlags(&_secFlags, flags) _secFlags = (flags);

#define SecSetFlags(flags) _SecSetFlags(&_secFlags, flags)

#define SecCatch(result, flags, error, ...) } _SecCatch(&_secResult, &_secFlags, &_secName, &_secError, &_secChain, &_secActions, result, flags, error,__VA_ARGS__)

// Add pending errors to *error  Clears any pending errors, and will log anything that was marked as needing to be logged.
#define SecFinally(result, flags, error, ...) _SecFinally(&_secResult, &_secFlags, &_secName, &_secError, &_secChain, &_secActions, result, flags, error,  __VA_ARGS__)

// Boolean function result
#define SecOk(result, flags, format, ...) _SecOK()

// Log action and it's arguments into the action log.
#if DEBUG
#define SecDebugAction(flags, action...)  SecAction(flags, action...)
#else
#define SecDebugAction(flags, action,...)
#endif

#define SecThrow(result, domain, flags, body, format...) { rtype _r = body(); __security_trace_return_api(__FUNCTION__, format, _r); return _r; }


#define SecEnd(rtype, body, error, format...) { rtype _r = body(); __security_trace_return_api(__FUNCTION__, format, _r); return _r; }

// Internal USE only DO NOT USE directly
#define _SecClearMask(flags, mask) (((flags) | (mask)) ^ (mask))
#define _SecLogLevel(level) (((level) << 0) & kSecLogLevelMask)
#define _SecLogStyle(style) (((style) << 4) & kSecLogStyleMask)

__BEGIN_DECLS

enum SecFlagEnum {
    kSecNoFlag              = 0,            // No flags, no logging nada
    kSecLogLevelMask        = (15 <<  0),   // Bits 0-3 contain the log levels 1-15 (since 0 is no flags).

    kSecFirstLogLevel    = _SecLogLevel(1), // Lowest log level
    kSecDebugLogLevel    = _SecLogLevel(1), // log secinfo
    kSecInfoLogLevel     = _SecLogLevel(2), // log info
    kSecNoticeLogLevel   = _SecLogLevel(3), // log notice
    kSecWarningLogLevel  = _SecLogLevel(4), // log warning
    kSecErrorLogLevel    = _SecLogLevel(5), // log error
    kSecCriticalLogLevel = _SecLogLevel(6), // log critical
    kSecAlertLogLevel    = _SecLogLevel(7), // log alert
    kSecLastLogLevel     = _SecLogLevel(15),// Max available log level.

    kSecLogStyleMask     = ( 0x30),         // Bits 4-5 are used to store log style chhoices.  The choice is yours.
    kSecLogPlainStyle    = _SecLogStyle(0), // Log plain message in code only no built in function names.
    kSecLogFunctionStyle = _SecLogStyle(1), // Log full __FUNCTION_NAME__
    kSecLogPrettyFuncStyle=_SecLogStyle(2), // Log full ___PRETTY_FUNCTION__
    kSecLogNameStyle     = _SecLogStyle(3), // Log name argument to SecWith()

    kSecFlagMask            = ( 0xFFC0), // Bits 4-16 are option flags and can be ored together with |
    kSecFirstFlag           = ( 1 << 6), // First flag defined

    kSecTraceFlag           = ( 1 <<  6), // trace this api call
    kSecChainFlag           = ( 1 <<  7), // chain multiple errors together in a array with the last error Enclosing all the others.
    kSecFlagAssert          = ( 1 <<  8), // assert that result is not fail without an error having been thrown
    kSecSafeModeFlag        = ( 1 <<  9), // Do not evaluate format arguments to avoid infinite recursion.
    kSecClearPendingFlag    = ( 1 << 10), // Clear any pending errors.
    kSecLogDisabledFlag     = ( 1 << 11), // Logging is disabled.
    kSecLogAlwaysFlag       = ( 1 << 12), // always log regardless of success or failure
    kSecLogEveryActionFlag  = ( 1 << 13), // log every action
    kSecReservedFlag        = ( 1 << 14), // Reserved for future use.
    kSecLastFlag            = ( 1 << 15), // Reserved for future use.


    kSecActionsMask         = (15 << 16), // Bits 4-16 are option flags and can be ored together with |
    kSecLowerLogLevelAction = ( 1 << 16), // Allow the log level to be lowered
    kSecTraceAction         = ( 1 << 17), // Trace this action.
    kSecReserved3Action     = ( 1 << 18), // Reserved for future use.
    kSecReserved4Action     = ( 1 << 19), // Reserved for future use.
    kSecReserved5Action     = ( 1 << 20), // Reserved for future use.
    kSecReserved6Action     = ( 1 << 21), // Reserved for future use.
    kSecReserved7Action     = ( 1 << 22), // Reserved for future use.
    kSecReserved8Action     = ( 1 << 23), // Reserved for future use.
    kSecReserved9Action     = ( 1 << 24), // Reserved for future use.
    kSecReserved10Action    = ( 1 << 25), // Reserved for future use.
    kSecReserved11Action    = ( 1 << 26), // Reserved for future use.
    kSecReserved12Action    = ( 1 << 27), // Reserved for future use.
    kSecReserved13Action    = ( 1 << 28), // Reserved for future use.
    kSecReserved14Action    = ( 1 << 29), // Reserved for future use.
    kSecReserved14Action    = ( 1 << 30), // Reserved for future use.
    kSecLastAction          = ( 1 << 31), // The last action defined.

};
typedef uint32_t SecFlagType;

SecInline SecFlagType _SecFlags(flag, ...) {
    SecFlagType _flag = flag;
    va_list ap;
    va_start(ap, flag);
    SecFlagType nextFlag;
    while ((nextFlag = va_arg(ap, SecFlagType))) _flag |= nextFlag;
    va_end(ap);
    return _flag;
}

SecInline void _SecSetLogLevel(SecFlagType flags[1], SecFlagType newFlags) {
    SecFlagType newLevel = _SecLogLevel(newFlags);
    if (!newLevel || newFlags & kSecLowerLogLevelAction)
        *oldFlags = newLevel & _SecClearMask(newFlags, kSecActionsMask);
    else if (newLevel > _SecLogLevel(*oldFlags))
        *oldFlags = _SecClearMask(*oldFlags, kSecLogLevelMask) | newLevel;
        // Canot lower log level
}

SecInline void _SecAction(void *_secResult, void *flags, void *name, CFErrorRef *error, CFMutableArrayRef *chain, CFMutableStringRef *actions, SecFlagType flags, key, CFStringRef action, __VA_ARGS__) {
}

SecInline void _SecSetFlags(SecFlagType oldFlags[1], SecFlagType newFlags) {
    // Log level can't be lowered unless kSecLowerLogLevelAction is present in newFlags.
    newLevel = newFlags & kSecLogLevelMask
    if (!newLevel || newFlags & kSecLowerLogLevelAction)
        *oldFlags = newFlags & (kSecLogLevelMask | kSecFlagMask);
    else if (newLevel > _SecLogLevel(*oldFlags))
        *oldFlags = _SecClearMask(*oldFlags, kSecLogLevelMask)
    (_SecLogLevel(newFlags)) ? _SecClearMask(*oldFlags);
    *oldFlags |= newFlags;
}

SecInline void _SecEnableFlags(SecFlagType oldFlags[1], SecFlagType newFlags) {
    (_SecLogLevel(newFlags)) ? _SecClearMask(*oldFlags);
    *oldFlags |= newFlags;
}

SecInline void _SecDisableFlags(SecFlagType oldFlags[1], SecFlagType newFlags) {
}

__END_DECLS

#endif /* _UTILITIES_SECMETA_H_ */
